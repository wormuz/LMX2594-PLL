/* st7789.c — ST7789 bit-bang driver. Init sequence from stock reverse (0x08003EE2).
 * 8x16 font (ASCII 0x20..0x7E) in font8x16.inc. */
#include "st7789.h"
#include "board.h"

/* 9x18 pixel-perfect font (ASCII + Ukrainian), 2 bytes/row, GLYPH_H rows/glyph. */
static const uint8_t font[] = {
#include "font.inc"
};
struct fmap { uint16_t cp; uint16_t idx; };
static const struct fmap fmap[] = {
#include "font_map.inc"
};
#define FMAP_N (sizeof(fmap)/sizeof(fmap[0]))

/* codepoint -> glyph index (linear; table is small, ~161 entries) */
static int glyph_index(uint32_t cp)
{
    for (unsigned i = 0; i < FMAP_N; i++) if (fmap[i].cp == cp) return fmap[i].idx;
    return 0;   /* fallback: space */
}
/* decode one UTF-8 char, advance *p. Handles ASCII + 2-byte Cyrillic. */
static uint32_t utf8_next(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    uint32_t cp; int n;
    if (s[0] < 0x80)      { cp = s[0]; n = 1; }
    else if ((s[0]&0xE0)==0xC0){ cp = ((s[0]&0x1F)<<6)|(s[1]&0x3F); n = 2; }
    else if ((s[0]&0xF0)==0xE0){ cp = ((s[0]&0x0F)<<12)|((s[1]&0x3F)<<6)|(s[2]&0x3F); n = 3; }
    else { cp = ' '; n = 1; }
    *p += n;
    return cp;
}

static inline void sclk(int v){ if(v) LCD_PORT->BSRR=(1u<<LCD_SCLK_PIN); else LCD_PORT->BSRR=(1u<<(LCD_SCLK_PIN+16)); }
static inline void mosi(int v){ if(v) LCD_PORT->BSRR=(1u<<LCD_MOSI_PIN); else LCD_PORT->BSRR=(1u<<(LCD_MOSI_PIN+16)); }
static inline void dc(int v){ if(v) LCD_PORT->BSRR=(1u<<LCD_DC_PIN); else LCD_PORT->BSRR=(1u<<(LCD_DC_PIN+16)); }
static inline void rst(int v){ if(v) LCD_PORT->BSRR=(1u<<LCD_RST_PIN); else LCD_PORT->BSRR=(1u<<(LCD_RST_PIN+16)); }
static void dly(volatile int n){ while(n--) __NOP(); }

/* Match stock FUN_08002930 exactly: SCLK low -> set data -> SCLK high (rising latch). */
static void wr8(uint8_t b){
    for(int i=0;i<8;i++){
        sclk(0);
        mosi(b&0x80);
        sclk(1);
        b<<=1;
    }
}
static void cmd(uint8_t c){ dc(0); wr8(c); }
static void dat(uint8_t d){ dc(1); wr8(d); }

/* ST7789 init — EXACT copy of stock FUN_08003EC4 (verified working).
 * Format: cmd, nargs, args...  (no delay flags; delays handled in lcd_init). */
static const uint8_t init_seq[] = {
    0x36, 1, 0x70,
    0x3A, 1, 0x05,
    0xB2, 5, 0x1F,0x1F,0x00,0x33,0x33,
    0xB7, 1, 0x00,
    0xBB, 1, 0x3F,
    0xC0, 1, 0x2C,
    0xC2, 1, 0x01,
    0xC3, 1, 0x0F,
    0xC4, 1, 0x20,
    0xC6, 1, 0x13,
    0xD0, 2, 0xA4,0xA1,
    0xD6, 1, 0xA1,
    0xE0,14, 0xF0,0x06,0x0D,0x0B,0x0A,0x07,0x2E,0x43,0x45,0x38,0x14,0x13,0x25,0x29,
    0xE1,14, 0xF0,0x07,0x0A,0x08,0x07,0x23,0x2E,0x33,0x44,0x3A,0x16,0x17,0x26,0x2C,
    0xE4, 3, 0x1D,0x00,0x00,
    0x21, 0,
    0x29, 0,
    0x00                                     /* end */
};
static void ms_delay(int ms){ for(volatile int d=0; d<ms*9000; d++) { } }  /* ~ms @72MHz */

static void win(int x,int y,int w,int h){
    cmd(0x2A); dat(x>>8);dat(x&0xFF);dat((x+w-1)>>8);dat((x+w-1)&0xFF);
    cmd(0x2B); dat(y>>8);dat(y&0xFF);dat((y+h-1)>>8);dat((y+h-1)&0xFF);
    cmd(0x2C);
}

void lcd_init(void)
{
    /* stock reset (FUN_08003EC4): all pins high, RST low, 20ms, RST high, 20ms. */
    LCD_PORT->BSRR = 0x1E0;                   /* PB5,6,7,8 high */
    rst(0); ms_delay(20);                     /* RST low pulse */
    rst(1); ms_delay(20);
    cmd(0x11);                               /* SLPOUT */
    ms_delay(120);
    for (const uint8_t *p = init_seq; *p; ) {
        uint8_t c = *p++, n = *p++;
        cmd(c);
        for (int i=0;i<n;i++) dat(*p++);
    }
    lcd_fill(C_BLACK);
}

void lcd_fill_rect(int x,int y,int w,int h,uint16_t col){
    win(x,y,w,h); dc(1);
    for(int i=0;i<w*h;i++){ wr8(col>>8); wr8(col&0xFF); }
}
void lcd_fill(uint16_t c){ lcd_fill_rect(0,0,LCD_W,LCD_H,c); }

static void lcd_glyph(int x,int y,uint32_t cp,uint16_t fg,uint16_t bg,int inv){
    const uint8_t *g=&font[glyph_index(cp)*GLYPH_H*2];
    win(x,y,GLYPH_W,GLYPH_H); dc(1);
    for(int r=0;r<GLYPH_H;r++){
        uint16_t row=((uint16_t)g[r*2]<<8)|g[r*2+1];   /* 16-bit, top GLYPH_W bits used */
        for(int b=0;b<GLYPH_W;b++){
            int on=(row&(0x8000>>b))!=0; if(inv) on=!on;
            uint16_t c=on?fg:bg; wr8(c>>8); wr8(c&0xFF);
        }
    }
}
int lcd_str(int x,int y,const char*s,uint16_t fg,uint16_t bg,int inv){
    while(*s){
        uint32_t cp=utf8_next(&s);
        lcd_glyph(x,y,cp,fg,bg,inv); x+=GLYPH_W;
        if(x>LCD_W-GLYPH_W){x=0;y+=GLYPH_H;}
    }
    return x;
}

/* 2x scaled glyph (each pixel -> 2x2 block) for big frequency readout. */
static void lcd_glyph2x(int x,int y,uint32_t cp,uint16_t fg,uint16_t bg){
    const uint8_t *g=&font[glyph_index(cp)*GLYPH_H*2];
    win(x,y,GLYPH_W*2,GLYPH_H*2); dc(1);
    for(int r=0;r<GLYPH_H;r++){
        uint16_t row=((uint16_t)g[r*2]<<8)|g[r*2+1];
        for(int sy=0;sy<2;sy++)
            for(int b=0;b<GLYPH_W;b++){
                uint16_t c=(row&(0x8000>>b))?fg:bg;
                wr8(c>>8);wr8(c&0xFF); wr8(c>>8);wr8(c&0xFF);   /* 2x horiz */
            }
    }
}
int lcd_str2x(int x,int y,const char*s,uint16_t fg,uint16_t bg){
    while(*s){ uint32_t cp=utf8_next(&s); lcd_glyph2x(x,y,cp,fg,bg); x+=GLYPH_W*2; }
    return x;
}
void lcd_hline(int y,uint16_t c){ lcd_fill_rect(0,y,LCD_W,1,c); }
