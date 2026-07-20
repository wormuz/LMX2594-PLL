Confirmed `_DAT_08006844` = RAM `0x20001d8c`, the UART command-line receive buffer, corroborating the CLI command parser is at UART main loop level, reachable any time regardless of `start`/`stop` state.

## Confirmed bugs (power/suspend/resume area)

**1. USB suspend leaves LMX2594 fully powered — RF output stays on (the known bug, root-caused precisely)**
- Addr: `0x080052ac` calls `FUN_08005224` (0x08005224-0x08005986), which only touches RCC/backup-domain registers (`DAT_080052b4` block, USB clock gating) and ends by calling `FUN_08002270` (just logs "usb enter low power mode" string + sets state flag `*0x20001ED0 = 3`).
- No call to `FUN_080069F4` (the SPI-write primitive) exists anywhere in `FUN_08005224`, `FUN_08002270`, or their callers/callees.
- Trigger: USB SUSPEND interrupt (bus goes idle >3ms).
- Symptom: R0.POWERDOWN stays 0, R44.OUTA_PD/OUTB_PD stay at init value (0) → VCO+dividers+output buffers stay active while board thinks it's "suspended" → overheat under sustained suspend (e.g., host sleep with cable attached).
- Fix: before/in `FUN_08005224`, before calling `FUN_08002270`, issue `FUN_080069f4` write to R44 setting OUTA_PD=OUTB_PD=1 (or R0 POWERDOWN=1), then on resume reissue the full `FUN_08002974` register burst (or at minimum R0 with POWERDOWN=0 and FCAL_EN=1) to restart cleanly.

**2. USB resume never reprograms LMX — relies on register retention with no verification**
- Addr: `0x08004848` calls `FUN_08004838` (0x08004838-0x08004858): clears one RCC/USB bit, calls `FUN_08003cc0` (only logs "leave low power mode" + sets state flag `*0x20001ED0` to 1 or 5 depending on `*(iVar1+10)`), sets `*DAT_08004858 = 0xbf00`.
- No call to `FUN_08002974` (full init) or `FUN_08005f50` (frequency/FCAL retrigger) anywhere in the resume path.
- Trigger: USB RESUME interrupt after any suspend.
- Symptom: if bug #1 is fixed and suspend actually powers down R0/R44, resume as currently coded would leave the synthesizer powered down forever — the two bugs compound: today it "works" only because suspend is a no-op for the RF path. Fixing #1 without also fixing #2 breaks output-on-resume.
- Fix: resume handler must call `FUN_080069f4` to clear POWERDOWN/OUTx_PD, then re-trigger FCAL (write R0 with FCAL_EN=1, i.e. same literal `0x251c` used elsewhere) per datasheet VCO-recal requirement after any power-down/up cycle.

**3. CLI `stop` command (UART, in `FUN_08006410` main loop, string match `_DAT_08006844=="stop"`) leaves LMX fully powered**
- Addr: inside `FUN_08006410`, the `stop` branch (`*_DAT_08006844=='s','t','o','p'`) only sets `*DAT_08006824 = 0` (sweep-enable flag) and prints "STOP" to LCD/UART — zero `FUN_080069f4` calls.
- Trigger: sending `stop\r` over UART (USART3) or equivalent CLI action.
- Symptom: user believes output is stopped (LCD says "STOP"), but LMX output/VCO stays fully enabled and driving the RF output — same overheat/leakage risk as bug #1, reachable without any USB suspend at all, purely from the TTL-UART command channel.
- Fix: `stop` handler should assert R44 OUTA_PD/OUTB_PD (or R0 POWERDOWN) via `FUN_080069f4`; `start` should clear them and retrigger FCAL before resuming the sweep/frequency loop.

**4. `wv` (output-power) UART command: unclamped 2-digit input corrupts reserved bit 14 of R44**
- Addr: `FUN_08006410`, `wv` branch: `*_DAT_080068ec = (digit3-'0') + (digit2-'0')*10` — accepts ASCII 00-99 with no upper bound check (valid OUTA_PWR range is 0-63 per 6-bit field).
- Consumed in `FUN_08005f50` at `FUN_080069f4(DAT_080062a0 | param_2 << 8)` where `DAT_080062a0 = 0x002C0023` (R44 base pattern, MASH_RESET_N=1, MASH_ORDER=3).
- Trigger: `wv64` through `wv99` over UART.
- Symptom: values 64-99 (bit 6 set in the 7-bit decimal-decoded value) shift into bit 14 of R44, a reserved/fixed bit outside the documented OUTA_PWR[13:8] field — undefined R44 state, potential to disturb MASH_RESET_N/MASH_ORDER-adjacent bits depending on exact carry, causing incorrect output power or fractional-N misbehavior.
- Fix: clamp parsed `wv` value to 0-63 before storing to `_DAT_080068ec`.

All four are independently confirmed via decompilation/listing; no speculation. Bugs 1-3 are directly in the "device runs hot/output never truly disables" bug family requested; bug 4 is a distinct input-validation defect found in the same code path (`FUN_08006410` UART CLI) while tracing register writes.