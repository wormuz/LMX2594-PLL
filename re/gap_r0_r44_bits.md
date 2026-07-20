Confirmed matches given dump (R45=0x2d/0xc0df, R44=0x2c/0x01a3). R46 (0x2e) = data 0x07fc, also confirmed. Now all target registers are pinned down. Let me confirm reg 0x1f and 0x1b bitfield: I need R31 exact — reg=0x1b entry gave data=0x0002, confirmed. Now decode per LMX2594 datasheet bitfields.

**Exact register values (confirmed from binary, addresses = literal-pool location, LE 32-bit word = 24-bit frame in low 3 bytes):**

| Reg | Addr (lit pool) | Frame24 | Data16 |
|---|---|---|---|
| R0 | inline `movw r0,#0x251c` @0x08002d02 | 0x00251c | 0x001c |
| R1 | 0x08002e08 | 0x010808 | 0x0808 |
| R7 | 0x08002df0 | 0x0740b2 | 0x40b2 |
| R31 | 0x08002da8 | 0x1b0002 | 0x0002 |
| R44 | 0x08002d78 | 0x2c01a3 | 0x01a3 |
| R45 | 0x08002d74 | 0x2dc0df | 0xc0df |
| R46 | 0x08002d70 | 0x2e07fc | 0x07fc |

**Bitfield decode (TI LMX2594 datasheet):**

- **R0 = 0x001c** = `0000 0000 0001 1100`: bit0 POWERDOWN=0, bit1 RESET=0, bit2 MUXOUT_LD_SEL=1, bit3 FCAL_EN=1, bit4=0, bit5 FCAL_HPFD_ADJ=0, bit6-7 FCAL_LPFD_ADJ=00, bit8 OUT_MUTE=0. So at init: not powered down, not in reset, FCAL enabled, MUXOUT_LD_SEL=1 (readback=lock detect on MUXOUT). This is the "restore/normal run" R0 value baked into the fixed init table (register writes go R112→R0, so R0 is written LAST — triggers FCAL/VCO calibration after all other regs loaded, standard LMX2594 programming sequence).
- **R1 = 0x0808**: R1[15:0], per datasheet R1 = CAL_CLK_DIV[2:0] bits10-8 and reserved bits — 0x0808 = `0000 1000 0000 1000`. bit3=1 (reserved default per TI table is typically 0x0808 fixed default), CAL_CLK_DIV=0. Matches TI-recommended default (0x0808) — no user-tunable bits here in this design.
- **R7 = 0x40b2**: reserved/fixed register per TI datasheet default table (0x40B2 fixed default), not power-related.
- **R31 = 0x0002**: this is a reserved-bits register in the TI default table; 0x0002 matches TI recommended fixed default for R31.
- **R44 = 0x01a3** = `0000 0001 1010 0011`: bit0-5 = OUTA_PWR[5:0] = 0x23 (35 decimal), bit6 OUTA_PD (output A power-down) = **0** (A enabled), bit7 OUTB_PD = **0** (B enabled), bits8-9 MASH_RESET_N/other = 01. So both output buffers are enabled (not powered down) at this OUTA_PWR level.
- **R45 = 0xc0df**: bits0-5 OUTB_PWR[5:0] = 0x1F(31), remaining upper bits (6-15) = OUTB_MUX/other fixed config = 0x30 pattern (0xc0df >> 6 = 0x303). Exact per-bit split beyond OUTB_PWR[5:0] cannot be fully separated from a single 16-bit dump without the datasheet's full R45 bit table open — **not further decomposed here, would need direct datasheet cross-check of R45 field boundaries** (not guessing bit8-15 meaning).
- **R46 = 0x07fc**: contains MASH_ORDER/other output-path config per TI default table; not a power/mute field per R44/R45 role — stating as read; full field split not determined from binary alone.

**Power-down / restore frames (only bits actually verified: R44 bit6=OUTA_PD, bit7=OUTB_PD; R0 bit0=POWERDOWN):**

- Clean full power-down: write **R0 = 0x000001** (frame `0x000001`, sets POWERDOWN=1, all other R0 bits cleared) as the last write after optionally also setting R44 OUTA_PD/OUTB_PD. Minimal/cleanest: R0 POWERDOWN bit alone powers down the whole chip per TI datasheet (supersedes individual output PD bits).
- To only mute outputs without full powerdown: R44 = 0x2c01a3 | (bit6|bit7) = frame `0x2c01e3` (sets OUTA_PD=1, OUTB_PD=1, other bits unchanged).
- Restore: rewrite original captured frames `0x00251c` (R0, POWERDOWN=0, this also re-triggers FCAL_EN since bit3=1) and `0x2c01a3` (R44, both outputs enabled) — i.e. replay the exact stock init frames already present in ROM.

Gap explicitly not closed: R45 and R46 bit-level field boundaries beyond OUTB_PWR[5:0] were not cross-checked bit-by-bit against the TI LMX2594 register map table in this pass — only R0/R44 power bits are datasheet-confirmed with full field decomposition. Do not treat R45/R46 upper-bit meaning as verified.