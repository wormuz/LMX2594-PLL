No GPIOC literal exists anywhere (confirmed: GPIOC base 0x40011000 never appears in the firmware — GPIOC is completely unused). No "CE" string label. This confirms LMX2594 is wired only through GPIOA (1 pin: PA3) and GPIOB (SPI/LCD/buttons) — no GPIOC involvement, no RESET-labeled symbol in firmware.

I have enough to conclude definitively.

## Findings

**Unexplained/leftover output pins found (exhaustive survey of every GPIOA/GPIOB CRL/CRH + BSRR/BRR write in the firmware):**

| Pin | Where configured | Where driven | Final state | Ever cleared/toggled again |
|---|---|---|---|---|
| **PA3** | `0x080029b8` (FUN_08002974, mode=output) | SET high at `0x080029c0` (`FUN_080024da(GPIOA,8)`), CLEARED low at `0x080029de` (`FUN_080024de(GPIOA,8)`) | **LOW** | No — no other GPIOA BSRR/BRR write exists anywhere in the 64KB image |
| **PB10** | `0x080029d6` (FUN_08002974, mode=output) | Never — no BSRR/BRR write to bit 0x400 found anywhere in the firmware | LOW (ODR hardware-reset default, mode set to output but never driven) | No |

Both pins are configured inside `FUN_08002974` (the LMX register-init function that also writes R112..R0 to the LMX2594 via `FUN_080069f4`), in the instruction window `0x080029b8-0x080029fa`, immediately before the first `FUN_080069f4(0x700000)` SPI write at `0x08002a02`. `FUN_08002974` itself is called exactly once, unconditionally, from the boot chain (`0x08000000` reset handler → `0x080002aa` → `FUN_08006410` main loop entry → `0x08006474`). It is never called again — no wake/resume path re-enters it (confirmed: only one reference to `0x08002974` in the whole binary; the known suspend callback `FUN_08002270` does not call it either).

**GPIOA usage overall**: GPIOA base literal (`0x40010800`) appears at only 4 sites in the whole firmware. Three are pure IDR reads for button polling (PA0/1/2/8, in `FUN_08001ca8` and `FUN_0800266c`). The fourth (`0x08002d10`, referenced at `0x080029b8/c0/de`) is PA3, output mode, set-then-cleared once, in `FUN_08002974`.

**GPIOC**: base literal (`0x40011000`) never appears anywhere in the firmware — GPIOC is completely unused.

**RESET pin**: no separate reset-line write pattern (LOW-then-HIGH pulse, which is the conventional RESET polarity) exists for LMX2594; PA3's pulse is HIGH-then-LOW, and PB10 is never pulsed at all. No string label ("CE"/"RESET"/"PD") is present in the firmware to corroborate either pin's intended function.

**Conclusion**: There is a firmware-controllable candidate line — **PA3**, pulsed high then driven low once at boot, immediately preceding the LMX SPI register-init sequence — plus a second unaccounted output-configured pin, **PB10**, which is never actually driven (stuck at reset-default low). Neither pin is touched again anywhere in the firmware after boot: there is no code path that could raise/lower either pin later (no wake, no menu action, no runtime function references the GPIOA base pointer or PB10's BSRR/BRR bit a second time). This rules out software-controlled power-down/wake via these pins post-boot — whatever PA3 or PB10 physically connect to (if anything on the LMX2594/EVAL schematic), firmware never revisits them after the one-time pulse/parked-low state at startup.

**Answer to yes/no**: Based purely on the binary — **there is no firmware-controllable CE/PD hardware line to the LMX2594 that firmware can toggle at runtime.** PA3 is driven once at boot (high then low, ending low) and never referenced again; PB10 is configured as output but never driven at all. If either pin is physically wired to LMX2594 CE on the schematic (unconfirmed — no schematic available, no string/label evidence in the binary), firmware provides no mechanism to raise it again after the initial boot pulse, so it cannot be used for a software wake-from-full-shutdown scheme without a firmware change. This is the definitive limit of what the binary proves; I have not seen the EVAL board schematic, so I cannot confirm which physical LMX2594 pin (if any) PA3/PB10 connect to.