Confirmed: `DAT_08005f4c` = literal `0x0800D000` — an internal flash address in the STM32F103's flash region (0x08000000 range, well above the 64KiB code area, in the flash's upper pages used as EEPROM-emulation/settings storage). This is **not an external chip interface at all**.

`FUN_08004684` (called once in CLI init, `FUN_08006410`) reads back this same 14-byte block via `FUN_0800488c(DAT_08004750, &local_20, 0xe)` — a flash read — to restore saved settings on boot (freq1, freq2, span, output-power, sweep-flag byte), falling back to `FUN_08005ec4(0,0,0,0,0)` (erase/zero the block) if it reads as unprogrammed (`0xff` sentinel).

## Findings for GAP "fun05ec4"

**FUN_08005ec4** (0x08005ec4, body correct as originally quoted): packs `param_1..param_4` (4×32-bit, big-endian byte order into buffer) + `param_5` (1 byte) into a 14-byte (0xe) stack buffer, then calls `FUN_080048b4(DAT_08005f4c, &buf, 0xe)`.

**DAT_08005f4c** = literal constant `0x0800D000`, read from the instruction pool right after the `bl FUN_080048b4` at 0x08005f40 (visible in listing: `08005f4c 00 d0 00 08` = `0800D000h`). That address is inside the MCU's own internal flash address space (STM32F103, 0x08000000 base), at offset 0xD000 — i.e., a page in the upper flash region reserved as non-volatile settings storage, separate from the 0x08000000-0x0800ffff `ram`/code block referenced in the task (0xD000 offset would be beyond 64KiB — this device likely has ≥64KiB flash or this is a larger-flash variant; regardless it is unambiguously an **internal-flash address literal**, not a GPIO/peripheral register and not an SPI/I2C target).

**FUN_080048b4(param_1,param_2,param_3)**: guards `param_1` in range `(0x7fffffff, DAT_08004994)` — wait, actual check is `0x7ffffff < param_1 && param_1 < DAT_08004994` (param_1 must be > 0x07FFFFFF, i.e., a flash-space address, and below some upper bound DAT_08004994). It then:
- calls `FUN_08002374()` — flash unlock (standard STM32 FLASH_KEYR sequence, not decompiled here but named-pattern-consistent)
- computes `uVar2 = (param_1 + 0xf8000000) >> 10` and `uVar4 = (param_1+0xf8000000 & 0x3ff)>>1` — page index and half-word offset within a 1KiB page (STM32F103 has 1KiB flash pages)
- reads the target page into a RAM buffer `DAT_08004998` via `FUN_0800488c` (flash-read-to-buffer, length 0x200 = 512 half-words = 1KiB)
- checks if the target region is already erased (`0xffff`); if not, erases the page (`FUN_080022a0`) then merges new data into the RAM copy and reprograms the whole page (`FUN_0800499c`)
- if already erased, writes directly (`FUN_0800499c(param_1, param_2, uVar1)`)
- loops across multiple pages if length spans page boundary
- calls `FUN_08002320()` — flash lock (mirrors unlock)

This is a **classic STM32 internal-flash "erase-page/merge/reprogram" driver**, i.e., software EEPROM emulation. `FUN_0800488c` = flash read helper, `FUN_0800499c` = flash program helper (likely FLASH_ProgramHalfWord loop), `FUN_08002374`/`FUN_08002320` = FLASH unlock/lock.

**Call sites of FUN_08005ec4** (3 total, confirmed via references_to):
1. `0x0800428c` — inside `FUN_080040a8` (menu/encoder handler), `iVar2==5` branch: `FUN_08005ec4(*DAT_0800443c, *DAT_08004440, *DAT_08004444, *DAT_0800444c, *DAT_08004454)` — saves 4 menu-edited values + 1 byte flag to flash when a menu item is confirmed/toggled.
2. `0x080046cc`... actually resolves inside `FUN_08004684`: `FUN_08005ec4(0,0,0,0,0)` — called only as a **fallback reset-to-zero** when the flash-stored settings block reads as blank (`0xff`) on boot, i.e., first-boot initialization of the settings block.
3. `0x080068e6` — inside `FUN_08006410` (main CLI loop), after handling `w1/w2/w3/wt/wv` commands: `FUN_08005ec4(*DAT_080068f8, *DAT_08006908, *DAT_08006904, *DAT_080068fc, *_DAT_080068ec)` — persists freq1/freq2/span/output-power/sweep-flag to flash after every CLI-driven parameter change.

**Conclusion**: FUN_08005ec4/FUN_080048b4 is **NOT an SPI/I2C/LMX/LCD interface**. It is the device's non-volatile parameter-storage routine, writing a 14-byte settings record (freq1, freq2, span, output power/flag byte) to internal STM32 flash page at 0x0800D000, using standard erase-merge-reprogram flash-emulation logic. The earlier report's claim that it's "some other iface, likely a 2nd chip/LCD" is wrong on the interface type but was correct that it is **not** an LMX SPI write — it's flash storage, confirmed by the address literal 0x0800D000 (internal flash space) and by `FUN_080048b4`'s flash-page arithmetic/unlock/erase/lock sequence. No GPIO bit-bang, no second chip, no LCD controller involved.