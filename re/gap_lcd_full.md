LCD subsystem fully reverse engineered.

**Controller identity: ST7789** (confirmed, not guessed) — init command sequence at 0x08003ee2-0x080040a2 uses register opcodes 0xB2 (PORCTRL), 0xB7 (GCTRL), 0xBB (VCOMS), 0xC0 (LCMCTRL), 0xC2 (VDVVRHEN), 0xC3 (VRHS), 0xC4 (VDVS), 0xC6 (FRCTRL2), 0xD0 (PWCTRL1), 0xD6, 0xE0/0xE1 (14-byte PVGAMCTRL/NVGAMCTRL gamma tables) — these opcodes/names are unique to Sitronix ST7789, absent from ST7735/ILI9341/HD44780. Standard sequence: RESET pulse (GPIOB mask 0x1e0 then 0x80, delays 20ms/120ms) → 0x11 SLPOUT → 0x36 MADCTL=0x70 → 0x3A COLMOD=0x05 (16bpp/RGB565) → gamma/power tables → 0x21 INVON → 0x29 DISPON.

**Interface**: bit-bang SPI on GPIOB (0x40010c00, same port as the LMX2594 SPI), all pin/timing distinct from the LMX bit-bang. `FUN_08002930(uint param_1)` = 8-bit MSB-first shifter using mask 0x20 (SCLK) and 0x40 (MOSI/DATA), BSRR-set via `FUN_080024de(base,mask)` and BRR-reset via `FUN_080024da(base,mask)`. `FUN_08002914(cmd)` = pulls DC low (mask 0x100 via `DAT_0800292c`) then shifts cmd byte → command write. `FUN_080028f8(data)` = pulls DC high then shifts byte → parameter/data write. `FUN_080028d4(pixel16)` = pushes a 16-bit RGB565 pixel (calls the 8-bit shifter twice, high byte then low byte), toggling CS via `DAT_080028f4`.

**Function map**:
- `FUN_0800194c(x0,y0,x1,y1)` — CASET(0x2A)+RASET(0x2B)+RAMWR(0x2C), the standard MIPI-DCS window-set + write-start.
- `FUN_08002760(color16)` — full-screen fill: sets window (0,0)-(0xef,0xef) i.e. 240x240, then pushes 0xf0*0xf0=57600 pixels of `color16`. Confirms 240x240 panel.
- `FUN_080027f0(x,y,ch,invert)` — glyph blit. Font table base `DAT_080028cc = 0x08007A00` (in FLASH), indexed `(ch-0x20)*0x40` bytes/glyph = 64 bytes = 32 rows x 2 bytes (16px-wide bitmap rows). Glyph cell rendered into a 16x32-pixel window via `FUN_0800194c`. Foreground color `*DAT_080028d0` (RAM 0x20001C68 region), background restored from saved `*DAT_080028c8`.
- `FUN_0800278c(x,y)` — single-pixel plot (used by invert/dot-mode path in `FUN_080027f0`).
- `FUN_080027ac(col,row,string)` — string renderer: iterates chars, calls `FUN_080027f0` per glyph, advances col by 0xE (14px) with wraparound at col>0xE0 (row+=0x10) and screen-bottom wrap at row>0xE0 (clears screen white 0xf800... actually color arg 0xf800 = red in RGB565, set once at splash start `*DAT_08002e90=0xf800`). Row/col params passed to `FUN_080027ac` in the splash function are literal pixel Y-offsets (0, 0x3c, 0x78, 0xb4, 0xc8, 0x28, 0x50, 0xa0, etc.) not text-grid rows — this is a pixel-addressed text layout, not a fixed char grid.

**Splash/menu screen** — function `FUN_08002e0c`, fully decompiled:
```
*DAT_08002e90 = 0xf800   // foreground color = red (RGB565)
*DAT_08002e94 = 0        // background color = black
FUN_080027ac(7,0,   "   LMX2594-EVAL  ")
FUN_080027ac(7,0x3c, "Rang 20M-15.5GHz")
FUN_080027ac(7,0x78, "   Step:1KHz    ")
FUN_080027ac(7,0xb4, "  Version-1.0   ")
FUN_080063a8(5000)      // delay ~5s
FUN_08002760(0)         // clear screen to black
FUN_080027ac(7,0,   "F1:00,000,000KHz")
FUN_080027ac(7,0x28, "F2:00,000,000KHz")
FUN_080027ac(7,0x50, "St:00,000,000KHz")
FUN_080027ac(7,0x78, "M:Start T:0000ms")
FUN_080027ac(7,0xa0, "OUTPOWER:00     ")
FUN_080027ac(7,200,  "   LMX2594-EVAL  ")
```
String literals live in FLASH at 0x08002e97-0x08002f48 (verbatim, all null-terminated, 16-17 bytes each). `FUN_08003480` (previously reported) is the live-update path for this same menu screen — it sprintf-formats F1/F2/step/OUTPOWER numeric fields in place at fixed offsets into the same string buffers, then rewrites "START"/"RUN "/"ERROR"/"SPOT " status word and re-renders all 5 lines via `FUN_080027ac`.

**Pin summary** (GPIOB 0x40010C00, same port as LMX2594 SPI, distinct bitmask):
- Reset: mask 0x1e0 set, then 0x80 (with 20ms/120ms delays) during panel init — 0x80 likely dedicated LCD-RST pin, 0x1e0 covers RST+other lines held during power-up.
- DC/RS (command vs data): mask 0x100.
- SCLK: mask 0x20.
- MOSI: mask 0x40.
- CS: `DAT_080028f4` (separate BSRR/BRR pointer used only in `FUN_080028d4` pixel push — value not yet dumped, same GPIOB region).

**Not determined from binary**: exact CS pin value at `DAT_080028f4` and precise value at `DAT_0800292c` beyond "mask 0x100 region" were inferred from code pattern (BSRR/BRR write-target = GPIOB base, consistent across all LCD accessors) but the literal pointer constant for CS was not individually re-dumped in this pass — if needed, read `0x080028f4` (4 bytes) directly to confirm it equals 0x40010c00 with a distinct mask, consistent with the others.

No HD44780/1602 character-LCD code path exists anywhere in this function set — all evidence (CASET/RASET/RAMWR framing, RGB565 16-bit pixel color values 0xf800, ST7789-specific gamma/power opcodes, 240x240 fill dimensions) is internally consistent for an ST7789 SPI TFT, confirmed not guessed.