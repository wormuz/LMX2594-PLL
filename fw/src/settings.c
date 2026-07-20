/* settings.c — flash-journaling persistence on STM32F103 (no EEPROM).
 * Last flash page = array of settings slots; newest valid slot wins.
 * Deferred write (3s idle) + explicit save. See research/ui-ux-and-persistence.md. */
#include "settings.h"
#include "board.h"
#include <string.h>

/* Last 1KB page of 64KB flash reserved for settings. F103 page = 1KB. */
#define NVM_PAGE_ADDR 0x0800FC00u
#define NVM_PAGE_SIZE 1024u
#define SLOT_SIZE     ((sizeof(settings_t) + 3u) & ~3u)   /* word-aligned */
#define SLOT_COUNT    (NVM_PAGE_SIZE / SLOT_SIZE)

settings_t g_set;

static bool     dirty;
static uint32_t dirty_since;
#define COMMIT_DELAY_MS 3000u

/* CRC-16-CCITT (poly 0x1021, init 0xFFFF) over all bytes except the crc16 field. */
static uint16_t crc16(const uint8_t *p, uint32_t n)
{
    uint16_t c = 0xFFFF;
    while (n--) {
        c ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++) c = (c & 0x8000) ? (c << 1) ^ 0x1021 : (c << 1);
    }
    return c;
}
static uint16_t settings_crc(const settings_t *s)
{
    return crc16((const uint8_t *)s, offsetof(settings_t, crc16));
}

static void defaults(void)
{
    memset(&g_set, 0, sizeof g_set);
    g_set.magic = SETTINGS_MAGIC; g_set.version = SETTINGS_VERSION; g_set.length = sizeof(settings_t);
    g_set.f_lo_khz = 2400000; g_set.f1_khz = 2000000; g_set.f2_khz = 4000000;
    g_set.step_khz = 10000;   g_set.dwell_ms = 100;
    g_set.outa_pwr = 31; g_set.outb_pwr = 31; g_set.outa_en = 0; g_set.outb_en = 0; g_set.sweep_mode = 0;
}

static const settings_t *slot(unsigned i){ return (const settings_t *)(NVM_PAGE_ADDR + i * SLOT_SIZE); }

void settings_load(void)
{
    const settings_t *best = 0;
    for (unsigned i = 0; i < SLOT_COUNT; i++) {
        const settings_t *s = slot(i);
        if (s->magic == SETTINGS_MAGIC && s->crc16 == settings_crc(s)) best = s;  /* last valid */
    }
    if (best) memcpy(&g_set, best, sizeof g_set);
    else      defaults();
    dirty = false;
}

/* ---- flash primitives (F103 FPEC) ---- */
static void flash_unlock(void){ if (FLASH->CR & FLASH_CR_LOCK){ FLASH->KEYR = 0x45670123; FLASH->KEYR = 0xCDEF89AB; } }
static void flash_lock(void){ FLASH->CR |= FLASH_CR_LOCK; }
static void flash_wait(void){ while (FLASH->SR & FLASH_SR_BSY){} }
static void flash_erase_page(uint32_t addr)
{
    flash_wait(); FLASH->CR |= FLASH_CR_PER; FLASH->AR = addr; FLASH->CR |= FLASH_CR_STRT;
    flash_wait(); FLASH->CR &= ~FLASH_CR_PER;
}
static void flash_write_hw(uint32_t addr, uint16_t hw)
{
    flash_wait(); FLASH->CR |= FLASH_CR_PG; *(volatile uint16_t *)addr = hw; flash_wait(); FLASH->CR &= ~FLASH_CR_PG;
}

/* find first fully-erased slot (all 0xFF); if none, erase page and use slot 0. */
static int find_free_slot(void)
{
    for (unsigned i = 0; i < SLOT_COUNT; i++) {
        const uint32_t *w = (const uint32_t *)slot(i); bool free = true;
        for (unsigned k = 0; k < SLOT_SIZE/4; k++) if (w[k] != 0xFFFFFFFFu) { free = false; break; }
        if (free) return (int)i;
    }
    return -1;
}

void settings_save_now(void)
{
    g_set.magic = SETTINGS_MAGIC; g_set.version = SETTINGS_VERSION; g_set.length = sizeof(settings_t);
    g_set.crc16 = settings_crc(&g_set);

    flash_unlock();
    int idx = find_free_slot();
    if (idx < 0) { flash_erase_page(NVM_PAGE_ADDR); idx = 0; }
    uint32_t addr = NVM_PAGE_ADDR + (uint32_t)idx * SLOT_SIZE;
    const uint16_t *src = (const uint16_t *)&g_set;
    for (unsigned k = 0; k < SLOT_SIZE/2; k++)
        flash_write_hw(addr + k*2, (k < sizeof(g_set)/2) ? src[k] : 0xFFFF);
    flash_lock();
    dirty = false;
}

void settings_mark_dirty(void){ dirty = true; }  /* dirty_since set by tick on first sight */

void settings_tick(uint32_t now_ms)
{
    static bool prev;
    if (dirty && !prev) dirty_since = now_ms;      /* rising edge: start timer */
    prev = dirty;
    if (dirty && (now_ms - dirty_since) >= COMMIT_DELAY_MS) settings_save_now();
}
