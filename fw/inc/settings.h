/* settings.h — persistent device state (RAM shadow + flash journaling). */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

#define SETTINGS_MAGIC   0x5A4D4C58u  /* 'XLMZ' */
#define SETTINGS_VERSION 1u

/* All frequencies in kHz (legacy CLI contract: w1..w3 = 8 digits kHz). */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t f_lo_khz;    /* current LO frequency */
    uint32_t f1_khz;      /* sweep start */
    uint32_t f2_khz;      /* sweep stop  */
    uint32_t step_khz;    /* sweep step  */
    uint16_t dwell_ms;    /* sweep dwell */
    uint8_t  outa_pwr;    /* 0..63 */
    uint8_t  outb_pwr;    /* 0..63 */
    uint8_t  outa_en;     /* Down-conv on/off */
    uint8_t  outb_en;     /* Up-conv on/off   */
    uint8_t  sweep_mode;  /* 0 off, 1 running */
    uint8_t  _pad;
    uint16_t crc16;       /* CRC-16-CCITT over [magic..._pad] */
} settings_t;

extern settings_t g_set;   /* live RAM shadow */

void settings_load(void);         /* read newest valid slot, else defaults */
void settings_mark_dirty(void);   /* schedule deferred flash write */
void settings_save_now(void);     /* commit immediately (explicit save/STOP) */
void settings_tick(uint32_t now_ms); /* call from superloop: deferred commit */

#endif /* SETTINGS_H */
