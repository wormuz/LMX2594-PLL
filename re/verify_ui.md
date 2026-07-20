Verification result: the single claimed UI bug is CONFIRMED.

Evidence chain checked directly against listing/decompile in `firmware.bin`:
- `0x080040b6`: `bl 0x080062e0` executes in the RIGHT (code3) branch of `FUN_080040a8` before `*0x20001D08` (field index, `*DAT_08004434`) is updated at `0x080040ba-0x080040d4`.
- `0x08004412`: same pattern in the LEFT (code1) branch, `bl 0x080062e0` before index update at `0x08004416-0x0800442c`.
- Both branches fall through to `LAB_0800442e` (`0x0800442e`: `bl 0x0800690c`), called unconditionally after the index mutation.
- `FUN_080062e0` and `FUN_0800690c` decompile to identical logic, using the same bitmap pointer `0x080091C0` and X-table `0x20001C90` (confirmed in each function's literal pool at `0x08006354/0x0800635c` and `0x080069ec/0x080069f0`).
- Pixel-plot chain `FUN_0800278c` -> `FUN_0800194c` (CASET 0x2A / RASET 0x2B, 1x1 window) -> `FUN_080028d4` (RAMWR 0x2C, single fixed color) only sets pixels for glyph bits equal to 1; no fill/rectangle-clear/XOR/invert exists anywhere in this call chain.

Net effect matches the claim exactly: each LEFT/RIGHT press draws the cursor glyph at the old field index, then draws it again at the new index, with no erase step, leaving stale glyphs on the LCD.

The other four "ruled out" items in the report were not independently re-verified in this pass (not requested — task scope was to verify the confirmed-bug claim). Findings reported via ReportFindings with verdict CONFIRMED.