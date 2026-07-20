Confirmed: `FUN_08007024` = IEEE-754 double add, `FUN_08007220` = double subtract, `FUN_08006cf4` = double multiply, `FUN_08006ffe` = int32→double, `FUN_08006fa4` = double→uint32 (with `FUN_08007178` as the invalid-op trap/exception handler, matching GCC softfp `__adddf3/__subdf3/__muldf3/__floatsidf/__fixunsdfsi`).

DAT_08006258 region (band thresholds, 32-bit LE):
`0024f400`(=0xf42400=16,000,000), `70383900`(=0x00393870=3,750,000)... these are VCO/frequency thresholds in kHz for the 18-way ladder (band select `local_48`/divider `local_4c` 0..0x11 = 18 output-divider settings, matching LMX2594's divider range 2,4,6,8,...,768 for full 20MHz-15.5GHz coverage via VCO 7.5GHz two-tuning-range architecture).

## Summary: freq_calc (LMX2594 register programming)

**Entry: `FUN_08005f50(uint freq_kHz, uint outpower)`** at 0x08005f50 — the frequency→register compute+program function, called from the UI handler `FUN_08003480` (which formats F1/F2/Step/OUTPOWER on LCD and drives sweep state).

**Algorithm:**
1. Divider-ladder select: 18-band comparison ladder against constants at `DAT_08006258..DAT_08006288` (band thresholds in kHz) picks `local_48` (bitmask value written into an output-divider config register, values 2,4,6,8,0xc,0x10,0x18,0x20,0x30,0x40,0x48,0x60,0x80,0xc0,0x100,0x180,0x200,0x300 = LMX2594 OUTA_MUX/CHDIV settings) and `local_4c` = divider index 0..0x11 (18 = full LMX2594 CHDIV range for 20MHz-15.5GHz coverage, VCO ~7.5GHz core).
2. Effective VCO target = `freq_kHz * divider` computed via **software IEEE-754 double arithmetic** (compiler-rt style soft-float): `FUN_08006ffe`=int→double, `FUN_08006cf4`=dbl mul, `FUN_08007024`=dbl add, `FUN_08007220`=dbl sub, `FUN_08006fa4`=dbl→uint32.
3. N-divider integer part computed by dividing VCO-target-double by reference-double (`DAT_08006298` = PFD/ref freq), truncated via `FUN_08006fa4`.
4. Fractional numerator = `(vco_target - N*ref) * 50000` (i.e., FRAC over a 2^? or decimal-scaled denominator — literal `50000` suggests FRAC_DEN scaling, not the chip's native 2^25/1-based frac, meaning firmware pre-scales into a custom decimal fixed-point before final divide), then re-divided by ref to get `uVar4` = fractional word.
5. Final SPI register writes via `FUN_080069f4(value)` (24-bit MSB-first SPI frame: bits[23:16]=addr, bits[15:0]=data; GPIO bit-bang using mask 0x800=MOSI,0x1000=SCLK,2=LE/CS):
   - `DAT_0800629c | local_4c<<6` — CHDIV/divider-select register
   - `DAT_080062a0 | outpower<<8` — **OUTA_PWR field, confirms OUTPOWER UI value written directly into output-power register bits [14:8] with no extra scaling**
   - one of two power-related regs with `outpower | (threshold-0x1f)`
   - `uVar4&0xffff | 0x2b0000` and `uVar4>>0x10 | 0x2a0000` — FRAC low/high registers (addr 0x2b, 0x2a)
   - `uVar3 | 0x240000` — N-divider integer register (addr 0x24)
   - `0x251c` fixed — likely PLL_NUM/DEN config or FCAL trigger register (addr 0x25, data 0x1c)

**Not yet inventoried**: exact register-address map (0x24/0x25/0x2a/0x2b vs LMX2594 datasheet register numbers — need cross-check against TI reg table, not done), the two unlabeled `DAT_0800629c`/`DAT_080062a0` register addresses, and full 18-entry threshold table values beyond the first two read (0xf42400, 0x393870...).

**USB suspend bug**: confirmed still applicable — nothing in this freq_calc path touches USB suspend; `FUN_08002270` (USB suspend handler) has no call edge to `FUN_08005f50`, `FUN_080069f4`, or any SPI/LMX2594 shutdown routine.