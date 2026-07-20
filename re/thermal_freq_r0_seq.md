Confirmed: FUN_08006cf4/FUN_08006ffe/FUN_08006fa4/FUN_08007024/FUN_08007220 are IEEE-754 soft-float division/multiplication/scaling routines (fixed-point double→int conversion), used to compute the N/frac and CHDIV*50000 float math for the VCO frequency plan. No SPI writes happen inside them — all SPI writes are the explicit `FUN_080069f4(...)` calls in FUN_08005f50 itself. This confirms the full SPI write sequence is exactly the 8 calls visible in the tail listing, in this exact program order.

## Result: DIG "freq_r0_seq" — FUN_08005f50 tail 0x0800614A–0x0800624C

Full decompiled function is `FUN_08005f50(uint param_1, uint param_2)` — `param_1` = target VCO frequency value, `param_2` = output power/amplitude field. The block 0x0800614A-0x0800624C is literally the back half of this same function (there is no separate function starting exactly at 0x0800614A; it's mid-function, reached via branch at 0x08006154).

**Exact SPI write order (8 `FUN_080069f4` calls, each = one 24-bit LMX frame, bit-banged MSB-first CS/PB1, CLK/PB12, DATA/PB11):**

1. `0x0800614A/6150` — R45 (0x2d) or conditionally chosen: writes `DAT_08006294`=0x2dc0df OR `DAT_0800628c`=0x2dc8df depending on `bVar1` (whether a CHDIV/divider path is active). This is **R45**.
2. `0x08006150` — `DAT_08006290 - 1` = R46 (0x2e) with `DATA-1`. This is **R46**.
   (These two, 1-2, are only reached via the `bVar1==false` branch at 0x0800614A; the `bVar1==true` branch at 0x0800616e/0x08006172 writes the *other* R45 variant then R46 unmodified minus 1 — both paths converge at 0x0800617a.)
3. `0x080061f6/0x080061fa` — **R75 (0x4b)**: `DAT_0800629c | (local_4c << 6)` — writes the CHDIV select field (`local_4c` = divider index 0..0x11 chosen by the frequency-range ladder above).
4. `0x080061fe/0x08006204` — **R44 (0x2c)**: `DAT_080062a0 | (param_2 << 8)` — writes output-power field into R44 (same register the thermal facts identify as OUTA_PD/OUTB_PD bits, here only the power-level byte is touched, not the PD bits).
5. `0x0800620e-0x08006216` or `0x0800621e-0x08006226` (branch on `local_4c` sp+0x40 nonzero) — **R44 again**, second write: `(DAT_0800628c or DAT_08006294) - 0x1f) | param_2` — a second R44 frame, again only power-field, selecting between two R44 base constants depending on divider path.
6. `0x0800622c/08006230` — **R43 (0x2b)**: `local_2c-slot | 0x2b0000` — upper 16 bits of PLL_NUM (fractional numerator high word).
7. `0x08006236/0800623a` — **R42 (0x2a)**: `local_30-slot | 0x2a0000` — lower 16 bits of PLL_NUM.
8. `0x08006240/08006244` — **R36 (0x24)**: `uVar3(N divider) | 0x240000` — integer N divider value.
9. `0x08006248/0800624c` — **R0 (0x00)**, fixed literal **`0x251c`** (data=0x251c, addr=0).

**Order relative to N/NUM/DEN: NUM_high(R43) → NUM_low(R42) → N(R36) → R0(0x251c) last.** DEN is not written here as a runtime register — LMX2594 PLL_DEN (R38/39/40 range) must already be set by the init block (0x08002A02..0x08002D06) and is not touched by this runtime function; only CHDIV(R75), OUTA power(R44 x2), PLL_NUM(R43,R42), PLL_N(R36), then R0 are rewritten per frequency change.

**R0 = 0x251c decoded (16-bit data field, LMX2594 R0 bit layout):**
bit0=0 (POWERDOWN=0, chip stays powered), bit1=0, bit2=1, bit3=1, bit4=1, bit8=1, bit10=1, bit13=1. Per TI LMX2594 register map, R0 bit0 is POWERDOWN and bit3 is FCAL_EN — **bit3=1 here means FCAL_EN is set on every runtime frequency write**, i.e. this R0 write **does re-trigger VCO calibration (FCAL)** each time the frequency is programmed. POWERDOWN(bit0) is explicitly 0 in this literal, confirming the runtime path never powers the chip down — it only ever re-arms FCAL after loading new N/NUM.

**Wake-and-reprogram implication:** to wake from the thermal-fix powerdown (R0 bit0=1 written elsewhere) and get a working output again, the correct sequence per this proof is: write R75 (CHDIV) → R44 (power, x2 frames) → R43 (NUM hi) → R42 (NUM lo) → R36 (N) → **R0 last, with bit0=0 and bit3=1 (i.e. literal 0x251c or equivalent with POWERDOWN cleared and FCAL_EN set)** to both un-power-down and re-trigger calibration in the same final frame. Writing R0 before N/NUM/CHDIV would calibrate against stale N/NUM — the binary's own order confirms R0 must be last.

No DEN/R38-40 rewrite occurs in this function — if DEN was left stale from before powerdown, this function alone will not fix it; that would require calling the init block or verifying DEN registers separately (not proven from this function; not claiming it either way).

Files/addresses referenced: firmware.bin @ 0x08005f50 (function entry), tail 0x0800614A-0x0800624C, constants at 0x0800628c/0x08006290/0x08006294/0x0800629c/0x080062a0, helper 0x080069f4 (bit-bang SPI), math helpers 0x08006cf4/0x08006ffe/0x08006fa4/0x08007024/0x08007220 (soft-float, no SPI side effects, verified by decompile).