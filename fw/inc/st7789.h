/* st7789.h — ST7789 240x240 RGB565, bit-bang. */
#ifndef ST7789_H
#define ST7789_H
#include <stdint.h>

#define LCD_W 240
#define LCD_H 240
#define RGB565(r,g,b) (uint16_t)(((r&0xF8)<<8)|((g&0xFC)<<3)|((b)>>3))
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_RED     0xF800
#define C_GREEN   0x07E0
#define C_GREY    0x8410
#define C_CYAN    0x07FF
#define C_YELLOW_ 0xFFE0

#define GLYPH_W 9
#define GLYPH_H 18

void lcd_init(void);
void lcd_fill(uint16_t color);
void lcd_fill_rect(int x, int y, int w, int h, uint16_t color);
/* s is UTF-8 (ASCII + Ukrainian Cyrillic). Returns x after last glyph. */
int  lcd_str(int x, int y, const char *s, uint16_t fg, uint16_t bg, int invert);   /* 1x, 9x18 */
int  lcd_str2x(int x, int y, const char *s, uint16_t fg, uint16_t bg);             /* 2x, 18x36 */
void lcd_hline(int y, uint16_t c);
#define CELL_W (GLYPH_W*2)   /* 18 */
#define CELL_H (GLYPH_H*2)   /* 36 */

#endif /* ST7789_H */
