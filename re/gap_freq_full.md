All addresses confirmed. Now full picture assembled — writing final structured answer.

**FUN_08005f50(param_1=freq_input_Hz, param_2=OUTA_MUX/pwr_field)** — reconstructed logic:

**CHDIV ladder** (thresholds compared directly against `param_1`, all plain uint32, no float conversion at compare time):

| range low (excl.) | range high (excl.) | local_48 (CHDIV field value written) | local_4c (index, → reg R75 bits) |
|---|---|---|---|
| 0x00393870 (3750000) | ∞(top) | 2 | (bVar1 only, local_4c stays 0 initially but overwritten below) |
| 0x001E8480 (2000000) | 0x00393870 (3750000) | 4 | 1 |
| 0x0013D620 (1300000) | 0x001E8480 (2000000) | 6 | 2 |
| 0x000E57E0 (940000) | 0x0013D620 (1300000) | 8 | 3 |
| 0x000A6040 (680000) | 0x000E57E0 (940000) | 0xC | 4 |
| 0x000737A8 (473000) | 0x000A6040 (680000) | 0x10 | 5 |
| 0x00053020 (340000) | 0x000737A8 (473000) | 0x18 | 6 |
| 0x00039DC8 (237000) | 0x00053020 (340000) | 0x20 | 7 |
| 0x00029040 (168000) | 0x00039DC8 (237000) | 0x30 | 8 |
| 0x00021B10 (138000) | 0x00029040 (168000) | 0x40 | 9 |
| 0x0001C520 (116000) | 0x00021B10 (138000) | 0x48 | 10 |
| 0x00014820 (84000) | 0x0001C520 (116000) | 0x60 | 11 |
| 0xe7a4 (59300) | 0x00014820 (84000) | 0x80 | 12 |
| 0xa21c (41500) | 0xe7a4 (59300) | 0xC0 | 13 |
| 0x73a0 (29600) | 0xa21c (41500) | 0x100 | 14 |
| 0x51a4 (20900) | 0x73a0 (29600) | 0x180 | 15 |
| 0x39d0 (14800) | 0x51a4 (20900) | 0x200 | 16 |
| 0x251c (9500) | 0x39d0 (14800) | 0x300 | 17 |

Threshold at `DAT_08006258`=0x00F42400 (16000000) only used as unconditional divide reference (`uVar2 = DAT_08006258` loaded once, unused after — dead read or used elsewhere). Guard `param_1 < 0x251c` (9500) → `bVar1` stays false → no CHDIV path taken, falls to VCO-direct else branch.

**Constants:**
- `DAT_0800628c` = 0x002DC8DF = 3000543 (decimal) — VCO-direct min bound / N-divider related constant used as `FUN_080069f4(DAT_0800628c)` when no CHDIV (bVar1=false)
- `DAT_08006290` = 0x002E07FD = 3016701 — second constant, `-1` variant used in both branches (`DAT_08006290 - 1`)
- `DAT_08006294` = 0x002DC0DF = 2998495 — CHDIV-path equivalent of DAT_0800628c
- `DAT_08006298` = 0x40E86A00 (used as the **high 32 bits of a double, low=0**) → double value = **50000.0** — this is the divide-by constant in `FUN_08006cf4(N, 0, DAT_08006298)`, i.e. **PFD/reference scale = 50000** (kHz, so 50 MHz reference expressed in kHz units, consistent with the `*50000` multiply seen later for the fractional part)
- `DAT_0800629c` = 0x004B0800 — OR-mask base for CHDIV register write: `reg = DAT_0800629c | (local_4c << 6)` → written via `FUN_080069f4`. Frame top byte 0x4B → **register address 0x4B** (matches known row R75=0x4b, data=0x08c0 from your R-list — consistent, this IS the CHDIV_DIV/OUTA_MUX/etc combined register, addr 0x4B, base data 0x0800, index shifted into bits [6:...])
- `DAT_080062a0` = 0x002C0023 — OR-mask base for second write: `reg = DAT_080062a0 | (param_2 << 8)` → register address 0x2C (matches R44=0x2c row you already have, base data 0x0023, param_2 injected at bit 8, i.e. OUTA_PWR/MUX field)

**Final write sequence** (all via `FUN_080069f4`, 24-bit frame = addr<<16 | data):
1. `reg 0x4B | (chdiv_index<<6)` (CHDIV select register)
2. `reg 0x2C | (param_2<<8)` (output power/mux register)
3. If CHDIV path (bVar1=true): `param_2 | (DAT_08006294 - 0x1f)` = `param_2 | 0x2DC0C0` → frame with reg byte = top byte of (0x2DC0DF-0x1F)=0x2DC0C0 → **reg 0x2D**, else (VCO direct): `param_2 | (DAT_0800628C - 0x1f)` = `param_2 | 0x2DC8C0` → also **reg 0x2D** (matches your R45=0x2d row, `data=0xc0df` — confirms this is R45 PLL_N MSB/related register, with the `-0x1f` being an N-divider pre-adjustment baked into the constant, and param_2 ORed into the low byte)
4. `0x2B0000 | (freq_word & 0xFFFF)` → **reg 0x2B**, low 16 bits of computed frequency word (FRAC or N low part)
5. `0x2A0000 | (freq_word >> 16)` → **reg 0x2A**, high 16 bits of same computed word
6. `0x240000 | uVar3` → **reg 0x24**, where `uVar3 = FUN_08006fa4(divide result)` — the FRAC numerator/denominator intermediate truncated-to-uint value
7. `movw r0,#0x251c ; bl FUN_080069f4` → literal frame **0x00251c**, i.e. **reg 0x25, data 0x001C** — a fixed trigger/apply write after the whole cluster (constant, not computed)

**Math**: `uVar6/uVar7` (freq as double via FUN_08006ffe) is divided by `DAT_08006298`-double (50000.0) using FUN_08006cf4 → truncated to uint (FUN_08006fa4) = `uVar3`. Then `uVar3*50000` is reconverted to double, subtracted/combined via FUN_08007220 (add/sub) against the CHDIV-adjusted frequency double, multiplied (FUN_08007024) by `FUN_08006ffe(uVar2)` (uVar2=DAT_08006258=16000000, unused-looking name but actually IS used as multiplier here), divided again by the 50000.0 constant, truncated to `uVar4` — split into `uVar4>>16` and `uVar4&0xFFFF` for reg 0x2A/0x2B (this is the **FRAC value split into two 16-bit registers**, matching LMX2594's real FRAC_MSB/FRAC_LSB register pair at addr 0x2A/0x2B). `uVar3` alone (the integer quotient before the FRAC remainder math) goes to reg 0x24, which is the **N-divider register** in the real LMX2594 map.

Everything above is read directly from decompiled code + raw hex at the cited addresses (0x08005f50–0x08006250, DAT block 0x08006258–0x080062a0). I could not further decompose what physical value `DAT_08006258`=16000000 and `DAT_08006290`=3016701/`DAT_0800628c`=3000543/`DAT_08006294`=2998495 represent semantically (e.g., exact VCO calibration constants) beyond their bit-level usage shown — the binary contains no strings/labels tying them to datasheet names, so I am not guessing further meaning for those three.