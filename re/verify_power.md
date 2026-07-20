All 4 claimed bugs CONFIRMED via decompilation, plus 1 factual correction to the background claim.

**Bug 1 (suspend leaves LMX on)**: CONFIRMED. `FUN_08005224`@0x08005224 (called from 0x080052ac) only writes PWR_CR (0x40005C40, backup-domain access bits) and calls `FUN_08002270` (log string + `*0x0800229c=3`). Zero `FUN_080069F4` calls anywhere in this path.

**Bug 2 (resume never reprograms LMX)**: CONFIRMED, with one wording correction. `0x08004848` is the `bl 0x08003cc0` instruction *inside* `FUN_08004838`, not a distinct external caller of it. `FUN_08004838` clears one PWR_CR bit, calls `FUN_08003cc0` (log + state flag), writes `0xbf00` to PWR_CR. No `FUN_08002974`/`FUN_08005f50` call exists here.

**Bug 3 (stop leaves LMX on)**: CONFIRMED, mechanism deeper than stated. The `stop` branch itself has zero `FUN_080069F4` calls (correct). But additionally: `FUN_08005f50` — which unconditionally ends with the full R0/R44 SPI burst including FCAL trigger `0x251c` — runs on **every main-loop iteration** regardless of the stop flag `*DAT_08006824` (that flag only gates sweep-increment logic, read back at 5 other addresses, none in the SPI path). So even a power-down write added to `stop` would be overwritten the next loop pass.

**Bug 4 (wv unclamped)**: CONFIRMED but narrower than claimed. Parsing matches (`buf[2]*10+buf[3]`, no clamp). Verified arithmetic: input 64→`0x4000` (bit14), 99→`0x6300` (bits 14,13,8) — bit14 is reserved (R44 reset `0x1FA3` has bit14=0). However the claim's hedge about disturbing `MASH_RESET_N`/`MASH_ORDER` is **not supported**: the OR/shift starts at bit8, so bits 0-5 are never touched for any input 0-99, and bit15 is never reached either. Real defect is isolated to reserved bit14 only.

**Correction to background claim**: `_DAT_08006844` pointer bytes in flash are `8c 1c 00 20` = RAM `0x20001c8c`, not `0x20001d8c` as stated.