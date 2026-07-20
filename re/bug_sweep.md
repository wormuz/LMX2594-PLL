**Sweep engine location**: the sweep state machine is not a separate function — it lives inside the combined UART-CLI/main-loop function `FUN_08006410` (0x08006410-0x08006b0c region). Key sweep globals: F1=`*DAT_0800681c`, F2=`*DAT_0800682c`, step=`*DAT_08006828`, sweep_en=`*DAT_08006824`, cur=`*DAT_08006834`.

Confirmed bugs (all substantiated by raw byte reads + whole-image cross-reference search, not decompiler artifacts):

1. **0x080064ec-0x08006502 — sweep advance is dead code.** Gated by `*DAT_08006838==1 && *DAT_0800683c==1`. `DAT_08006838` = literal pointer `0x20000000`; `DAT_0800683c` = literal pointer `0x42218128` (ARM bit-band alias region). Verified via `memory search` across the full 64KiB image: both pointer constants appear exactly once each — only at their own literal-pool slot at 0x08006838/0x0800683c — and are never the destination of any store instruction anywhere in the firmware. The AND can never be satisfied. Result: after `start`, cur/F1 is programmed once and the LMX never advances — the sweep does not sweep, ever, on any hardware state.

2. **0x080064bc-0x080064d6 — wrap-to-F1 is functionally moot** because of bug 1 (cur never reaches F2 to begin with), but as written it force-resets cur=F1 unconditionally every loop tick whenever cur==F2 && sweep_en, with no single-shot latch — would double-trigger/skip dwell if bug 1 were fixed in isolation.

3. **`stop` command handler** (string match `s,t,o,p` inside FUN_08006410) clears `sweep_en` to 0 but never resets `cur` back to F1 — leaves stale mid-sweep state; currently masked by bug 1 but a real gap that would surface immediately if bug 1 is fixed without also fixing this.

Not reported as confirmed: `FUN_08005f50`'s band-boundary table mixes inclusive (`<=`) linear-table thresholds with strict (`<`) hardcoded hex thresholds at the low end (0xe7a3, 0xa21b, etc.) — noticed but not proven to produce a wrong band without bench measurement, so excluded from the confirmed list per no-speculation rule.

Files/addresses of interest: `FUN_08006410` (0x08006410), `FUN_08005f50` (0x08005f50), `FUN_08003480` (0x08003480), literal pool at 0x08006838/0x0800683c, dead-code block 0x080064ec-0x08006502, wrap check 0x080064bc-0x080064d6.