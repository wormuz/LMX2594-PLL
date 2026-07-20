## UI/Menu Bug Hunt Results

**Confirmed bug: cursor-trail / no-erase on field navigation**

- Address: `0x080040b6` (RIGHT/code3 branch) and `0x08004412` (LEFT/code1 branch), inside `FUN_080040a8` (button-event dispatcher).
- What's wrong: `FUN_080062e0` (draw cursor glyph) is called with the field index `*DAT_08004434` still at its **old** value, before the index is incremented/decremented. After the index update, `FUN_0800690c` is called unconditionally at `0x0800442e` and draws the identical glyph (same bitmap `0x080091c0`, same X-table `0x20001c90`) at the **new** index. The underlying pixel primitive (`FUN_0800278c` → `FUN_0800194c` CASET/RASET 1x1 window → `FUN_080028d4` RAMWR) only writes pixels for glyph bits that are `1`; it never clears the background first. There is no fill/erase step anywhere in this call chain.
- Trigger: any LEFT or RIGHT button press that moves the field cursor.
- Symptom: the cursor arrow at the previously-selected field is never removed. Repeated navigation leaves multiple simultaneous stale cursor glyphs on the LCD, so the user cannot tell which field is actually selected.
- Suggested fix: before calling `FUN_0800690c`'s draw (or in `FUN_080062e0`), issue a small filled-rectangle "erase" (background-color fill) over the previous cursor's glyph cell prior to plotting the new one, or restructure so the cursor is erased at the old position (using background color) and drawn once at the new position rather than drawn twice.

**Checked and ruled out (no defect found):**
- Field-index wraparound (`0x1d`=29 as last of 30 fields, 0↔29 wrap on LEFT/RIGHT): correct, full 30-cycle, no off-by-one.
- Value increment/decrement bounds for all 5 field groups (freq A/B/C capped at `DAT_0800445c`=15,500,000; group 24-27 capped `<10000`; group 28-29 capped `<0x40`=64 for the 6-bit OUTx_PWR fields): all bound-checked symmetrically on both directions, no over/underflow.
- Button debounce (`FUN_0800266c`): single 10 ms blocking delay + resample, edge-triggered with release-latch (`*DAT_08002754`) — correctly prevents repeat-fire while held, no stuck state at boot (bss-zeroed flag safely waits for release).
- Suspected missing `R0`/FCAL_EN retrigger in `FUN_08005f50` on every menu edit — initially looked missing, but the trailing `FUN_080069f4(0x251c)` call decodes as `addr=0x00 (R0), data=0x251c` with bit3 (FCAL_EN) set, so VCO calibration **is** retriggered correctly on every edit. No bug.
- Apparent 5-argument call to `FUN_08003b3c` (4-parameter function) in the CENTER/ENTER branch — decompiler artifact; assembly at `0x08004272` confirms exactly 4 registers (r0-r3) are set, matching the real signature.

Relevant addresses for reference: `FUN_080040a8` (0x080040a8, button dispatcher), `FUN_0800266c` (button read/debounce), `FUN_080062e0`/`FUN_0800690c` (cursor draw, 0x080062e0/0x0800690c), `FUN_0800278c`/`FUN_0800194c`/`FUN_080028d4` (pixel plot primitives), `FUN_08005f50` (0x08005f50, register programming incl. R0 FCAL retrigger).