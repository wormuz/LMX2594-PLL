All three claims CONFIRMED against ground-truth decompile of `FUN_08006410` and byte/xref-level literal-pool inspection.

Ground-truth decompiled loop body (address 0x08006410 region, do-while):
```c
if ((cur == F2) && (sweep_en == '\x01')) { cur = F1; }                    // claim 2 block
if ((sweep_en=='\x01') && (cur<F2) && (*0x08006838=='\x01') && (*0x0800683c==1)) {
    *0x08006838 = '\0';
    cur = cur + step;
    FUN_08005f50(cur, *0x08006818);        // LMX reprogram — dead, never runs
    if (F2 <= cur) { cur = F1; }
}
```
`stop` handler: `sweep_en=0;` print — no write to `cur`. `start` handler: `sweep_en=1; FUN_08005f50(cur,...)` — no reset of `cur`.

Evidence gathered independently (not reused from claim text):
- Literal pool at 0x08006818-0x08006840 read directly: confirms `DAT_08006838=0x20000000`, `DAT_0800683c=0x42218128`.
- `references_to 0x08006838` → only READ xrefs at 0x080064ec, 0x080064fe. `references_to 0x0800683c` → only READ xref at 0x080064f4. Zero WRITE xrefs to either literal-pool slot in the whole program.
- `memory search hex "28 81 21 42"` (0x42218128 LE) → exactly one hit, its own literal-pool slot at 0x0800683c. No other code loads this constant, so nothing can ever target it with a store.
- Decompiled sweep-tick `if` places the LMX reprogram call inside the same dead gate — bug 1's effect is broader than "cur frozen": the per-step reprogram call itself never executes post-init, not just the increment.

All three claims stand as originally stated; no rejections. The excluded item (band-boundary `<=`/`<` mixing in FUN_08005f50) was correctly left out per no-speculation rule — not re-litigated here since it wasn't submitted as a confirmed claim.