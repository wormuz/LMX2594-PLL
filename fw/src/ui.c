/* ui.c — two-level UI: HOME dashboard -> MENU -> modal editors.
 * Ukrainian, 9x18 font. Selected line inverted. Full repaint on screen change
 * (no cursor-trail, fix B9). Sweep intentionally absent from UI (UART-only). */
#include "ui.h"
#include "board.h"
#include "st7789.h"
#include "version.h"
#include "app.h"
#include "settings.h"
#include "lmx2594.h"
#include <stdio.h>
#include <string.h>

#define ROW_H 20
#define Y0    30

typedef enum { SC_HOME, SC_MENU, SC_FREQ, SC_PWR, SC_SYS } screen_t;
static screen_t sc = SC_HOME;
static int sel;                 /* selected row index within screen */
static int dirty = 1;           /* full repaint needed */

/* frequency digit editor state */
static uint32_t edit_val;       /* value being edited, kHz */
static int edit_digit;          /* active digit index, 0 = most significant */
#define FREQ_DIGITS 8

/* ---- menu definition ---- */
static const char *MENU[] = {
    "Задати частоту", "Down-вихід", "Up-вихід",
    "Потужність виходів", "Зберегти налаштування",
    "Налаштування системи", "Назад",
};
#define MENU_N (int)(sizeof(MENU)/sizeof(MENU[0]))

static const char *SYS[] = {
    "Опора: 10 МГц", "UART: 115200", "При старті: OFF", "Яскравість: 80%",
    "Скинути", "Назад",
};
#define SYS_N (int)(sizeof(SYS)/sizeof(SYS[0]))

static uint32_t pow10u(int n){ uint32_t v=1; while(n--) v*=10; return v; }

/* format kHz with space thousands separators: 12450000 -> "12 450 000" */
static void fmt_khz(char *out, uint32_t v)
{
    char d[11]; int n = 0;
    do { d[n++] = '0' + v % 10; v /= 10; } while (v);
    int o = 0;
    for (int i = n - 1; i >= 0; i--) {
        out[o++] = d[i];
        if (i && (i % 3) == 0) out[o++] = ' ';
    }
    out[o] = 0;
}

void ui_mark_dirty(void){ dirty = 1; }

/* ---- painters (each does a full clear of its region) ---- */
static void paint_home(void)
{
    lcd_str(4, 2, lmx_locked() ? "LOCK" : "UNLOCK",
            lmx_locked()?C_GREEN:C_RED, C_BLACK, 0);
    lcd_str(120, 2, "UART", C_CYAN, C_BLACK, 0);
    lcd_hline(22, C_GREY);
    char b[48];   /* UTF-8 Cyrillic = 2 bytes/char, need headroom */
    lcd_str(66, 28, "Частота, кГц", C_GREY, C_BLACK, 0);
    /* fixed 11-char field "  15 000 000", right-aligned -> no leftover on shrink */
    char fs[16]; fmt_khz(fs, g_set.f_lo_khz);
    char fpad[16]; int fl=(int)strlen(fs); int pad=(11-fl>0)?(11-fl):0;
    for(int i=0;i<pad;i++) fpad[i]=' '; fpad[pad]=0; strcat(fpad,fs);
    lcd_str2x(12, 54, fpad, C_WHITE, C_BLACK);       /* fixed width, no re-center jitter */
    lcd_hline(98, C_GREY);
    snprintf(b,sizeof b,"Down-вихід: %s  %2u", g_set.outa_en?"ON ":"OFF", g_set.outa_pwr);
    lcd_str(6, 108, b, g_set.outa_en?C_GREEN:C_GREY, C_BLACK, 0);
    snprintf(b,sizeof b,"Up-вихід:   %s  %2u", g_set.outb_en?"ON ":"OFF", g_set.outb_pwr);
    lcd_str(6, 132, b, g_set.outb_en?C_GREEN:C_GREY, C_BLACK, 0);
    lcd_hline(160, C_GREY);
    extern int mcu_temp_c(void);
    snprintf(b,sizeof b,"Температура: %-4d C", mcu_temp_c());
    lcd_str(6, 172, b, C_GREY, C_BLACK, 0);
    lcd_hline(196, C_GREY);
    extern int mcu_vdd_mv(void);
    int vdd = mcu_vdd_mv();
    snprintf(b, sizeof b, "%d.%02dВ", vdd/1000, (vdd%1000)/10);   /* e.g. 3.28В */
    lcd_str(6, 206, b, vdd<3100?C_RED:C_GREY, C_BLACK, 0);       /* left-bottom */
    lcd_str(120, 206, "центр—меню", C_GREY, C_BLACK, 0);
}

static void paint_list(const char *title, const char *const *items, int n)
{
    lcd_str(80, 4, title, C_CYAN, C_BLACK, 0);
    lcd_hline(24, C_GREY);
    for (int i = 0; i < n; i++)
        lcd_str(6, Y0 + i*ROW_H, items[i], C_WHITE, C_BLACK, i==sel);
    lcd_hline(214, C_GREY);
    lcd_str(6, 220, "вліво — назад", C_GREY, C_BLACK, 0);
}

static void paint_freq(void)
{
    lcd_str(30, 6, "Задати частоту", C_CYAN, C_BLACK, 0);
    char b[FREQ_DIGITS+1];
    snprintf(b,sizeof b,"%08lu",(unsigned long)edit_val);
    int x=20, y=70;
    for (int i=0;i<FREQ_DIGITS;i++){
        char d[2]={b[i],0};
        lcd_str(x, y, d, i==edit_digit?C_WHITE:C_YELLOW_, C_BLACK, i==edit_digit);
        x += GLYPH_W+8;
    }
    lcd_str(80, 120, "кГц", C_GREY, C_BLACK, 0);
    char step[20]; snprintf(step,sizeof step,"крок: %lu", (unsigned long)pow10u(FREQ_DIGITS-1-edit_digit));
    lcd_str(60, 150, step, C_GREEN, C_BLACK, 0);
    lcd_str(6, 200, "L/R розряд  U/D змінити", C_GREY, C_BLACK, 0);
}

static void paint_menu(void)
{
    lcd_str(80, 4, "МЕНЮ", C_CYAN, C_BLACK, 0);
    lcd_hline(24, C_GREY);
    char b[48];
    for (int i = 0; i < MENU_N; i++) {
        const char *t = MENU[i];
        if (i == 1) { snprintf(b,sizeof b,"Down-вихід: %-3s", g_set.outa_en?"ON":"OFF"); t = b; }
        else if (i == 2) { snprintf(b,sizeof b,"Up-вихід:   %-3s", g_set.outb_en?"ON":"OFF"); t = b; }
        uint16_t fg = C_WHITE;
        if (i == 1) fg = g_set.outa_en ? C_GREEN : C_GREY;
        if (i == 2) fg = g_set.outb_en ? C_GREEN : C_GREY;
        lcd_str(6, Y0 + i*ROW_H, t, fg, C_BLACK, i==sel);
    }
    lcd_hline(214, C_GREY);
    lcd_str(6, 220, "вліво — назад", C_GREY, C_BLACK, 0);
}

static screen_t last_sc = 0xFF;
static void paint(void)
{
    if (sc != last_sc) { lcd_fill(C_BLACK); last_sc = sc; }  /* clear only on screen change */
    switch (sc) {
        case SC_HOME: paint_home(); break;
        case SC_MENU: paint_menu(); break;
        case SC_SYS:  paint_list("Система", SYS, SYS_N); break;
        case SC_FREQ: paint_freq(); break;
        case SC_PWR: {
            lcd_str(60, 4, "Потужність", C_CYAN, C_BLACK, 0);
            lcd_hline(24, C_GREY);
            char b[24];
            snprintf(b,sizeof b,"OUTA (Down): %2u", g_set.outa_pwr);
            lcd_str(6, Y0, b, C_WHITE, C_BLACK, sel==0);
            snprintf(b,sizeof b,"OUTB (Up):   %2u", g_set.outb_pwr);
            lcd_str(6, Y0+ROW_H, b, C_WHITE, C_BLACK, sel==1);
            lcd_str(6, Y0+2*ROW_H, "Назад", C_WHITE, C_BLACK, sel==2);
            lcd_hline(214, C_GREY);
            lcd_str(6, 220, "L/R змінити  вліво — назад", C_GREY, C_BLACK, 0);
            break;
        }
        default: break;
    }
    dirty = 0;
}

void ui_init(void)
{
    lcd_init();
    lcd_str(36, 80, "LMX2594-EVAL", C_RED, C_BLACK, 0);
    lcd_str(30, 110, "build " GIT_HASH, C_GREY, C_BLACK, 0);
    for (volatile int d=0; d<3000000; d++) { }     /* short splash (~0.3s) */
    sc = SC_HOME; sel = 0; dirty = 1;
}

/* commit edited frequency, clamp to range */
static void freq_commit(void)
{
    if (edit_val < LMX_FOUT_MIN_KHZ) edit_val = LMX_FOUT_MIN_KHZ;
    if (edit_val > LMX_FOUT_MAX_KHZ) edit_val = LMX_FOUT_MAX_KHZ;
    app_set_lo(edit_val);
}

void ui_handle(btn_t b)
{
    switch (sc) {
    case SC_HOME:
        if (b == BTN_CENTER) { sc = SC_MENU; sel = 0; dirty = 1; }
        break;

    case SC_MENU:
        if (b == BTN_UP)   { sel = (sel==0) ? MENU_N-1 : sel-1; dirty=1; }   /* wrap */
        if (b == BTN_DOWN) { sel = (sel==MENU_N-1) ? 0 : sel+1; dirty=1; }
        if (b == BTN_CENTER) {
            switch (sel) {
                case 0: edit_val=g_set.f_lo_khz; edit_digit=0; sc=SC_FREQ; break;
                case 1: app_output_enable(0, !g_set.outa_en); break;   /* Down toggle */
                case 2: app_output_enable(1, !g_set.outb_en); break;   /* Up toggle */
                case 3: sc=SC_PWR; sel=0; break;
                case 4: app_save(); break;
                case 5: sc=SC_SYS; sel=0; break;
                case 6: sc=SC_HOME; break;                             /* Назад */
            }
            dirty=1;
        }
        if (b == BTN_LEFT) { sc = SC_HOME; dirty=1; }   /* back */
        break;

    case SC_FREQ:
        if (b == BTN_LEFT)  { if (edit_digit>0) edit_digit--; dirty=1; }
        if (b == BTN_RIGHT) { if (edit_digit<FREQ_DIGITS-1) edit_digit++; dirty=1; }
        if (b == BTN_UP || b == BTN_DOWN) {
            uint32_t step = pow10u(FREQ_DIGITS-1-edit_digit);
            if (b==BTN_UP) edit_val += step; else if (edit_val>=step) edit_val -= step;
            if (edit_val > 99999999u) edit_val = 99999999u;
            dirty=1;
        }
        if (b == BTN_CENTER) { freq_commit(); sc=SC_MENU; sel=0; dirty=1; }
        break;

    case SC_PWR:
        if (b == BTN_UP)   { sel = (sel==0)?2:sel-1; dirty=1; }
        if (b == BTN_DOWN) { sel = (sel==2)?0:sel+1; dirty=1; }
        if (b == BTN_LEFT || b == BTN_RIGHT) {
            int d = (b==BTN_RIGHT)?+1:-1;
            if (sel==0) app_set_power(0,(uint8_t)((g_set.outa_pwr+d)&0x3F));
            else if (sel==1) app_set_power(1,(uint8_t)((g_set.outb_pwr+d)&0x3F));
            dirty=1;
        }
        if (b == BTN_CENTER && sel==2) { sc=SC_MENU; sel=0; dirty=1; }
        break;

    case SC_SYS:
        if (b == BTN_UP)   { sel = (sel==0)?SYS_N-1:sel-1; dirty=1; }
        if (b == BTN_DOWN) { sel = (sel==SYS_N-1)?0:sel+1; dirty=1; }
        if (b == BTN_CENTER) {
            if (sel==4) app_save();               /* reset placeholder */
            if (sel==5) { sc=SC_MENU; sel=0; }
            dirty=1;
        }
        if (b == BTN_LEFT) { sc=SC_MENU; sel=0; dirty=1; }
        break;
    default: break;
    }
}

void ui_refresh(void)
{
    if (dirty) paint();
}

/* framed alert overlay, ~5s, then force full repaint (also recovers LCD). */
void ui_alert(const char *msg)
{
    lcd_init();                              /* re-init in case panel browned out */
    lcd_fill(C_BLACK);
    /* frame */
    lcd_fill_rect(10, 80, LCD_W-20, 80, C_RED);
    lcd_fill_rect(14, 84, LCD_W-28, 72, C_BLACK);
    /* count UTF-8 chars (not bytes) for correct centering */
    int nch = 0; for (const char *s=msg; *s; s++) if ((*s & 0xC0) != 0x80) nch++;
    int w = nch * GLYPH_W;
    int x = (LCD_W - w)/2; if (x < 16) x = 16;
    lcd_str(x, 104, msg, C_RED, C_BLACK, 0);
    int n2 = 0; const char *m2 = "виходи вимкнено";
    for (const char *s=m2; *s; s++) if ((*s & 0xC0) != 0x80) n2++;
    lcd_str((LCD_W - n2*GLYPH_W)/2, 128, m2, C_WHITE, C_BLACK, 0);
    for (volatile int d=0; d<300000000; d++) { }   /* ~5s */
    dirty = 1;
    paint();
}
