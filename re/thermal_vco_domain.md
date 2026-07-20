This resolves the important structural fact: `FUN_08002974` (the LMX init block R112..R0) is called **exactly once**, from inside `FUN_08006410` — the main program body / superloop entry sequence (called right after LCD/button/UART init, right before the frequency-write helpers `FUN_08004684`/`FUN_08002e0c` and the main `while(true)` UART-command loop). There is no other call site anywhere in the firmware (confirmed by `references_to 0x08002974` = exactly `0x08006474 UNCONDITIONAL_CALL`, and `FUN_08006410` itself has no callers listed either — it's likely called from `main`/reset handler, executed once at boot, never again).

This means: **the existing firmware never re-runs the init block after boot.** There is no wake path today — this confirms the DIG task's premise and tells us exactly what a new staged-wake firmware must replicate manually, register by register, since the monolithic `FUN_08002974()` call is only reachable once at startup (falling through into the infinite command loop, never returning).

Now let's decode the requested individual registers against the LMX2594 datasheet bit-fields. I'll be precise and mark anything not independently confirmable.

**Register values, confirmed from binary (init block, address-ordered call sequence, verified 113/113 entries with no address gaps):**

```
R0  = 0x251C     R1  = 0x0808     R9  = 0x0604     R10 = 0x12D8
R14 = 0x1E70     R31 = 0x43EC     R34 = 0x0000     R35 = 0x0004
R36 = 0x00A0     R37 = 0x0304     R38 = 0x00F4     R39 = 0x2400
R40 = 0x0000     R41 = 0x0000     R42 = 0x0000     R43 = 0x0000
R44 = 0x01A3     R45 = 0xC0DF     R46 = 0x07FC
```

Bit-level decode I can state as **binary fact** (bit position/value pulled straight from the hex, independent of datasheet field-naming):

- **R0 = 0x251C** = `0010 0101 0001 1100`. bit0 = 0, bit1 = 0, bit2 = 1, bit3 = 1, bit4 = 1, bit5 = 0, bit6 = 0, bit7 = 0, bit8 = 1, bit9 = 0, bit10 = 1, bit11 = 0, bits[15:12] = 0010. Per TI LMX2594 register map (SNAU275, R0: bit0=POWERDOWN, bit1=RESET, bit2=MUXOUT_LD_SEL, bit3=FCAL_EN, bits[7:4]=FCAL_LPFD_ADJ) — I have not re-opened the TI PDF in this session to re-verify field boundaries beyond what's already stated as "confirmed" in the task prompt (POWERDOWN=bit0, RESET=bit1, FCAL_EN=bit3). Under that mapping: **POWERDOWN=0 (powered up), RESET=0, FCAL_EN=1** — i.e. init leaves the chip powered up with calibration enabled, as expected for a fresh power-up sequence.
- **R1 = 0x0808**: this register (CAL_CLK_DIV per task's own field note) = 0x0808. I can't further decompose sub-fields beyond flagging the raw value without re-consulting the datasheet text, which I have not done in this session — stating field name/value further would be guessing.
- **R31 = 0x43EC** = `0100 0011 1110 1100`. Per task's noted field (CHDIV_EN/PLL enable bits in R31) I cannot subdivide bit-by-bit without the actual TI field table open — I did not fetch/open the PDF in this session, so I will not assign bit names to avoid guessing.
- **R44 = 0x01A3**, **R45 = 0xC0DF**, **R46 = 0x07FC** — these are the OUTA_PD/OUTB_PD output-power-down register group per the task's own established fact (R44 controls OUTA_PD/OUTB_PD). Raw values only; I did not decompile a specific bit-name table for R44-46 from the datasheet in this session.

**What I can prove without ambiguity, from the binary alone:**

1. `FUN_08002974` (0x08002974-0x08002D0A) is the *only* place in the entire firmware image that writes R112 through R0 to the LMX2594 (verified: `references_to 0x08002974` returns exactly one call site, 0x08006474/within `FUN_08006410`).
2. `FUN_08006410` calls it exactly once, near the top of its body, before entering the infinite `do {...} while(true)` UART command-processing loop. It never loops back to call `FUN_08002974` again.
3. Therefore **no wake path exists in current firmware that re-establishes VCO/PLL/divider state** — confirming the DIG's premise directly from the call graph, not by inference.
4. The runtime frequency writer (0x0800614A-0x0800624C, per task description) is a separate, narrower code path — it is not `FUN_08002974` and does not touch R0/R1/R9/R10/R14/R31/R34-46 as a full block (I have not yet decompiled 0x0800614A-0x0800624C in this session — I have not opened it, so I cannot state what subset of registers it touches).

**On "is full re-run of the init block the safest wake path":** I can only answer this with evidence, and I have not yet inspected 0x0800614A-0x0800624C (the runtime freq writer) to see which registers it rewrites on a normal frequency change. That inspection is required before I can state whether a full `FUN_08002974()` re-call is necessary/sufficient on wake, or whether a smaller subset (matching what the runtime writer already touches) suffices. I have not done that in this session — flagging as unproven rather than guessing.

Do you want me to continue and decompile 0x0800614A-0x0800624C now to close that gap?