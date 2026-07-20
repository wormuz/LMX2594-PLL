This is FUN_08005f50, the runtime frequency-programming function. Confirmed decompiled: `FUN_080069f4(0x251c);` is the final unconditional call — every single invocation of this function (i.e., every frequency change) always ends by writing R0=0x251C, unconditionally (not inside the `if(bVar1)` branch — it's after, common path).

Findings are conclusive. Summary:

**R0 write inventory — complete, 2 sites total in the whole binary:**

1. **Init sequence, address 0x08002D02-0x08002D06** (inside the R112→R0 descending init block that starts at 0x08002A02):
   `movw r0,#0x251c` → `bl 0x080069F4` (FUN_080069f4, the bit-bang SPI writer).
   Data = 0x251C = binary `0010 0101 0001 1100`.
   - bit0 POWERDOWN = 0
   - bit1 RESET = 0
   - bit3 FCAL_EN = 1
   Context: final write of the init block (after R112..R1 written once each). Standard LMX2594 procedure — R0 written last to release RESET and trigger VCO calibration.

2. **Runtime frequency-change writer, address 0x08006248-0x0800624C**, inside `FUN_08005f50` (called from the freq-change dispatcher at 0x0800614A-0x0800624C):
   `movw r0,#0x251c` → `bl 0x080069F4`, decompiled as literal `FUN_080069f4(0x251c);`.
   Same data value 0x251C: POWERDOWN=0, RESET=0, FCAL_EN=1.
   This call is **unconditional** — outside the `if(bVar1){...}else{...}` branch that selects the divider path, so it executes on every single frequency change regardless of range.

**Is POWERDOWN ever set to 1 anywhere?** No. `mcp__ghidra__inspect references_to 0x080069F4` returned exactly 126 call sites, all falling within the two known ranges (init block 0x08002A02-0x08002D06, runtime writer 0x0800614A-0x0800624C). I enumerated every literal-pool value and every inline immediate feeding r0 across both ranges (dumped and decoded all 24-bit frames). Only two frames have addr-byte 0x00 (R0), both carry data=0x251C, both with POWERDOWN bit0=0. No other R0 write exists in the binary — confirmed by exhaustive cross-reference, not sampling.

**Does every freq change re-write R0 with POWERDOWN=0?** Yes, confirmed from decompiled `FUN_08005f50`: `FUN_080069f4(0x251c);` is the last statement in the function, unconditional, executed on every call regardless of the frequency-range branch taken. This means any future firmware modification that tries to set LMX2594 R0 bit0 (POWERDOWN=1) purely by writing R0 once would be undone on the very next frequency change, since this function unconditionally rewrites R0=0x251C (POWERDOWN=0) every time. A powerdown-on-idle feature would need to either skip calling `FUN_08005f50` while powered down, or patch this trailing R0 write to preserve/OR in the powerdown bit conditionally.

No R0 write with POWERDOWN=1 exists anywhere in the current firmware — this confirms LMX2594 thermal management via R0 bit0 is not implemented in the current binary at all; it would need to be added.

**Files/addresses referenced:**
- `firmware.bin`, function `FUN_080069f4` at 0x080069F4 (24-bit bit-bang SPI writer, confirmed via decompile: loop `for bVar1=0;bVar1<0x18` shifting `0x800000` down, calling GPIO helpers `FUN_080024da`/`FUN_080024de`)
- Init R0 write: 0x08002D02 (`movw r0,#0x251c`) / 0x08002D06 (`bl`)
- Runtime R0 write: 0x08006248 (`movw r0,#0x251c`) / 0x0800624C (`bl`), inside `FUN_08005f50` (entry 0x08005f50)
- Literal pool dumped and decoded: 0x08002D0C-0x08002E08 (all R112..R1 init values), 0x0800628C-0x080062A4 (freq-writer literals)