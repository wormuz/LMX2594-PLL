Confirms task's premise directly: `FUN_08002270` (USB suspend callback) only prints the string and sets `RAM[0x20001ED0]=3` (via `DAT_0800229c` pointer). No call to `FUN_080069f4` (LMX SPI write), `FUN_080069f4(0x2c...)`/R44 power-down, or any GPIO/LMX function. Confirmed: chip stays fully programmed/powered during USB suspend.

## Findings — lmx_regmap

**1. Low-level SPI write (bit-bang, no HW SPI used):**
`FUN_080069f4(uint param_1)` @ 0x080069f4 — writes 24 bits MSB-first to LMX2594. Uses GPIOB (base `0x40010c00`, `DAT_08006a68`). Pins via BSRR(+0x14)/BRR(+0x10) helpers `FUN_080024da`/`FUN_080024de`:
- CS/LE = bit `0x0002` (toggled low at start, high at end — LE pulse)
- CLK = bit `0x1000`
- DATA (MOSI) = bit `0x0800`
Loop: for `bVar1<24`, drive DATA bit from `param_1` (mask starts `0x800000`), pulse CLK. No hardware SPI1/SPI2 peripheral literal-pool reference exists anywhere in flash — confirms bit-bang, not SPI-peripheral.

**2. Register table / init sequence:**
Located 0x080029f0–0x08002d0a, one `bl 0x080069f4` per LMX2594 register, addr encoded in bits[23:16] (`{8-bit addr, 16-bit data}`), descending R112(0x70)→R37(0x25):
```
0x700000, 0x6f0000, 0x6e0000, ... 0x2b0000, 0x2a0000, 0x290000, 0x280000, ...
```
Most regs (R79,R78,R73-R76,R68,R65-R64 etc.) load fixed literal constants; some (R68 @0x2d20 area, R44/R45 @0x08002d5c/0x08002d60, R40/R39/R37 etc.) load from RAM variables (user-set output power / VCO / freq divider — populated by `FUN_08005f50` freq-calc). Table ends at R37 (`0x251c`) — R0-R36 not in this excerpt (written by a separate earlier block, not captured in this pass).

**3. R44/R45 (output power / OUTA_PD/OUTB_PD) and R0 (powerdown):**
- R44 (addr 0x2c) and R45 (0x2d) values are RAM-sourced (`ldr r0,[0x08002d5c]`/`[0x08002d60]`), i.e. dynamically written each time frequency/power is set — these are the OUTx_PWR/OUTx_PD fields, actively driven only during normal tuning, never touched from suspend.
- R0/R1 (POWERDOWN bit, addr 0x00/0x01) not present in the inspected 0x25–0x70 table segment; no other call site to `FUN_080069f4` with addr-byte 0x00 or 0x01 found in the full cross-reference list of 0x080069f4 callers (all callers enumerated map to addr 0x25–0x70, plus dynamic freq-set path 0x08005f50 using 0x24/0x2a/0x2b/0x29/0x1c-shifted).

**4. USB suspend callback — confirmed bug:**
`FUN_08002270` @ 0x08002270:
```c
void FUN_08002270(void)
{
  FUN_08000328(s_usb_enter_low_power_mode_08002280);
  *DAT_0800229c = 3;   // RAM[0x20001ED0] = 3
  return;
}
```
No reference to `FUN_080069f4` (SPI write) or any GPIO/LMX access. Confirmed: entering USB suspend never issues an SPI write to LMX2594 R0 (POWERDOWN) or R44/R45 (output power-down) — synth keeps running at full output, only a status flag is set and a log string printed.