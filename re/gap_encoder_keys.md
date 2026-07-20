Confirmed: only one memory block exists — flat "ram" 0x08000000-0x0800ffff (the flash itself, misleadingly named "ram" in Ghidra's block list per task setup). There is NO separate RAM block loaded at 0x20000000 in this Ghidra project — meaning any address in the 0x20000000 range is genuinely unbacked/inaccessible in this analysis, exactly as the read error showed. That confirms these are true SRAM runtime variables (menu index, step tables, current freq/div values) with no static initial content visible in the binary; their default values, if any, must come from actual `.data` LMA content still resident in the flash image before 0x08000000 block's boundaries. Given block is exactly 0x08000000-0x0800ffff (64KiB, matches known fact), and no second flash region holds `.data` copy source separately identified, this is as far as static analysis can go for these specific default numeric step values — cannot be guessed.

This is the last major structural fact needed. Reporting complete findings now.

## Encoder / Keys / Menu — Reversed

**GPIO pins (confirmed via FUN_0800266c @ 0x0800266c, using FUN_080024c8 @ 0x080024c8 = `(*(uint*)(base+8) & mask) != 0`, i.e. reads GPIOx_IDR):**

- `DAT_08002758` = `0x40010800` = **GPIOA** base (literal pool @0x08002758, confirmed by direct memory read)
- `DAT_0800275c` = `0x40010c00` = **GPIOB** base (literal pool @0x0800275c, same port as LMX SPI bit-bang)
- `DAT_08002754` = `0x20001c70` = pointer to a debounce/state flag byte in SRAM

Masks tested (all against IDR, i.e. `base+8`):
- GPIOA bit 8 (`0x100`) = PA8
- GPIOA bit 1 (`0x2`) = PA1
- GPIOA bit 0 (`0x1`) = PA0
- GPIOA bit 2 (`0x4`) = PA2
- GPIOB bit 15 (`0x8000`) = PB15

Logic (0x0800266c): first pass requires all 5 pins HIGH (idle, active-low inputs with pull-ups) before falling into "settle" branch. Once idle confirmed once, it forces the debounce flag to 0 (calls `FUN_080063a8(10)`, a 10-unit delay) then re-tests each pin in strict order, returning as soon as one reads LOW:
- PA8 low → return 1
- PA1 low → return 2
- PA0 low → return 3
- PA2 low → return 4
- PB15 low → return 5
- all high → return 0 (no event)

Mapped to FUN_080040a8 (menu/encoder handler @ 0x080040a8) dispatch:
- code 1 → `FUN_080062e0()` then menu index **decrement** (wraps 0→0x1d): this is one encoder rotation direction (PA8)
- code 3 → `FUN_080062e0()` then menu index **increment** (wraps 0x1d→0): other encoder rotation direction (PA0)
- code 2 → **decrement** the currently-selected field's value by its step (PA1) — held/short button, "down"
- code 4 → **increment** the currently-selected field's value by its step (PA2) — "up" button
- code 5 → **toggle** `*DAT_08004458` (0/1 byte) and re-apply frequency/output (PB15) — likely RF ON/OFF or apply/enter button

So PA8/PA0 = quadrature encoder A/B channels (rotation), PA1/PA2 = up/down (or coarse adjust) push-buttons, PB15 = a 5th button (enable/apply/toggle). Only one shared 10ms software delay (`FUN_080063a8(10)`) is used as debounce, applied once per detected-idle-then-active transition; there is no per-pin separate debounce counter.

**Menu index**: `*DAT_08004434` (SRAM 0x20001d08 per literal pool, though this exact RAM address is unbacked/unreadable in the static image — expected, it's runtime SRAM). Range enforced as 0..0x1d (0-29 decimal) = **30 field slots**, not 29 — index wraps at `== 0x1d` (29) back to 0, meaning valid indices are 0..29 inclusive = 30 states. (Earlier "29 fields" assumption is off by one; confirmed range is 0-29.)

**Field grouping** (from FUN_080040a8 bounds-checks): indices partition into 5 banks of size 8,8,8,8,3 approximately:
- idx 0-7 → adjusts `*DAT_0800443c` (SRAM var, "value0"/main frequency Hz), step from table `DAT_08004438[idx]`, then calls `FUN_08002f4c`
- idx 8-15 → adjusts `*DAT_08004440` ("value1"), step `DAT_08004438[idx-8]`, calls `FUN_08003108`
- idx 16-23 → adjusts `*DAT_08004444` ("value2"), step `DAT_08004438[idx-16]`, calls `FUN_080032c4`
- idx 24-27 → adjusts `*DAT_0800444c` ("value3", clamped <10000), step `DAT_08004448[idx-24]`, calls `FUN_08003c28`
- idx 28-29 → adjusts `*DAT_08004454` ("value4", clamped <0x40=64), step `DAT_08004450[idx-28]`, calls `FUN_08003af4`

This gives exactly 8+8+8+4+2 = 30 slots, matching the 0-29 index range.

After any edit, `FUN_08005f50(*DAT_0800443c, *DAT_08004454)` (the frequency-compute function) and `FUN_08003b3c(...)` (display update, presumably) are re-invoked. In the `iVar2==5` (PB15) branch, `FUN_08005ec4(*DAT_0800443c,*DAT_08004440,*DAT_08004444,*DAT_0800444c,*DAT_08004454)` is also called — this is the function already confirmed NOT to be an LMX write (packs a 14-byte buffer to a different interface, likely 2nd chip/LCD), so PB15 triggers both a frequency recompute AND an update to that other peripheral.

**Cannot determine from the binary (explicitly)**: the actual numeric contents of the three step-size tables `DAT_08004438` (used by idx 0-15), `DAT_08004448` (idx 24-27), `DAT_08004450` (idx 28-29), nor the initial/default values of value0-value4 and the menu index itself. These live in SRAM at 0x20001d0c/0x20001d34/0x20001d2c/0x20001c74/0x20001c78/0x20001c80/0x20001c84/0x20001c8c/0x20001c90/0x20001d08, and this Ghidra project has only one memory block (`ram`, actually the flash image 0x08000000-0x0800ffff, 64KiB) — no second block backs 0x20000000+, so those SRAM addresses are unreadable and their .data-section flash-resident initializer (if any) was not located in the searched region. Did not find the actual `.data` LMA copy source in flash. Cannot guess the digit/step values — would need either the actual `.data` init table in flash (not yet located) or live device readout.

**Not yet decompiled**: FUN_08002f4c, FUN_08003108, FUN_080032c4, FUN_08003c28, FUN_08003af4, FUN_08003b3c, FUN_08006ffe/08007024/08006cf4/08006fa4/08007220 (fixed-point helpers used by FUN_08005f50), FUN_080062e0 — did not inspect these in this pass; only their names/call sites confirmed via decompilation of the caller.