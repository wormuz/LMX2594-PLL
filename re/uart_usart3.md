## USART3 UART reverse-engineering result

**(1) Init** \u2014 `FUN_08006a6c` (0x08006a6c), called from `FUN_08006410` as `FUN_08006a6c(0x2580)`:
- `FUN_08004564(0x40000)`: RCC->APB1ENR |= bit18 (USART3EN)
- `FUN_08004584(8,1)`: RCC->APB2ENR |= bit3 (GPIOB clock)
- `FUN_080023b2` twice: configures GPIOB CRH \u2014 mask 0x400 (PB10) = AF push-pull 50MHz (**TX=PB10**), mask 0x800 (PB11) = input (**RX=PB11**)
- Baud: param = 0x2580 = **9600 decimal**. `FUN_0800570c` computes BRR from PCLK1 (queried via `FUN_080045a4`) \u2014 with PCLK1=36MHz, BRR=36000000/9600=3750 (exact, no fractional error)
- Word/parity: CR1 config fields all zero \u2192 **8 data bits, no parity, 1 stop bit** (default M=0/PCE=0)
- USART enabled via `FUN_080056c0`/`FUN_080055a0` (CR1 UE/TE/RE bits)

**(2) TX or RX+parser**: **Both exist in code, but only TX is actually reachable.**
- TX: `FUN_08006a6c` \u2192 byte-send primitive at 0x080063f0 (poll SR bit 0x40=TXE, write DR) reached via `FUN_08006b10`\u2192`FUN_08005bd8` ring buffer, called from the CLI's `start`/`stop`/`w1..wv` command echoes in `FUN_08006410`. TX is live.
- RX: full poller+parser exists at **0x08005518** (poll SR RXNE via `FUN_0800566c`, read DR via `FUN_080057e4`, byte masked `&0x1FF`), running the identical CRLF state-machine as the USB-CDC parser (flag bits 0x4000="saw CR", 0x8000="line complete"), writing into its own 200-byte buffer at RAM **0x200022DC** with completion flag at RAM **0x20001DA4**.
- **Dead code**: exhaustive call-graph closure of `FUN_08006410` (traced 3 levels, ~75 functions) contains zero edges into 0x08005518, and `references_to` on it returns no callers anywhere in the 64KB flash. No USART3 IRQ handler exists either \u2014 vector table slots for USART1/2/3/EXTI15-10 (offsets 0x98-0xA4) all point to the same generic default stub (0x080002DB). **The TTL-UART RX line is fully configured in hardware but never polled by any reachable code path** \u2014 an orphaned/incomplete feature, consistent with alpha status.

**(3) Command set / shared parser**: The USART3 RX parser (dead) and the USB-CDC RX parser (`FUN_08005b58`, also has zero direct callers but is reachable as a registered USB stack RX callback) use **separate flag/buffer pairs** (0x20001DA4/0x200022DC vs 0x20001DD8/0x2000288C \u2014 confirmed by literal-pool search, each address referenced exactly once, only inside its own handler). `FUN_08006410`'s command dispatch (`start`,`stop`,`w1`,`w2`,`w3`,`wt`,`wv`) only reads the USB-CDC flag (0x20001DD8) and buffer (0x2000288C). Even if USART3 RX were reachable, it feeds a buffer the CLI dispatcher never inspects \u2014 so there is no shared command set with USART3 today.

**(4) RX buffer/framing/terminator**: 200-byte buffer (index wraps at 199\u2192cleared to 0, mask via `ubfx #0,#14` i.e. 14-bit index against a 0x3FFF field), terminator = **CRLF** (`\r` sets "pending CR" bit, then `\n` sets "line ready" bit; any other byte after `\r` resets the whole accumulator to 0). Identical framing logic duplicated between the (dead) USART3 path and the USB-CDC path.

Key addresses: init `0x08006a6c`; TX byte primitive `0x080063f0`; TX GPIO/CR3 helper pool refs `0x08005594`,`0x0800640c`,`0x08006b0c` (all literal `0x40004800`); dead RX poller `0x08005518` (literal pool `0x08005598`\u21920x20001DA4, `0x0800559c`\u21920x200022DC); RXNE/TXE bit-test helper `0x0800566c`; DR read `0x080057e4`; live USB-CDC parser `0x08005b58` (pool `0x08005bd0`\u21920x20001DD8, `0x08005bd4`\u21920x2000288C, both also used by `0x08006410`'s command dispatch); vector table default-handler stub `0x080002DB` at slots 0x98-0xA4 (USART1/2/3/EXTI15_10 unimplemented).

**Alpha bug found**: USART3 RX command channel is entirely non-functional \u2014 hardware is initialized and the parser exists, but it is dead code with no caller and no ISR, so no command sent over the TTL-UART RX pin can ever reach the CLI.