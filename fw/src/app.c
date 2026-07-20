/* app.c — application logic: dual-output control, thermal FSM, sweep engine.
 * Fixes: B1 (idle powerdown), B4 (working sweep), keeps LMX + settings in sync.
 * See research/new-firmware-interface.md, thermal-and-powerdown.md. */
#include "app.h"
#include "ui.h"
#include "lmx2594.h"
#include "settings.h"
#include <stdio.h>

static app_state_t st;
static uint32_t sweep_cur_khz;
static uint32_t sweep_last_ms;

static bool any_output_on(void){ return g_set.outa_en || g_set.outb_en; }
int app_outputs_active(void){ return any_output_on() ? 1 : 0; }

/* apply thermal policy: chip on iff any output enabled, else deep powerdown. */
static void thermal_update(void)
{
    if (any_output_on()) {
        if (st == ST_IDLE_ALLOFF) {           /* wake */
            lmx_powerdown(false);
            lmx_set_freq_khz(g_set.f_lo_khz);  /* reprogram + FCAL */
        }
        st = g_set.sweep_mode ? ST_SWEEPING : ST_ACTIVE;
    } else {
        lmx_powerdown(true);                   /* both off -> ~2mA, cool */
        st = ST_IDLE_ALLOFF;
    }
}

static void apply_outputs(void)
{
    lmx_output_enable(LMX_OUTA, g_set.outa_en);
    lmx_output_enable(LMX_OUTB, g_set.outb_en);
    lmx_set_power(LMX_OUTA, g_set.outa_pwr);
    lmx_set_power(LMX_OUTB, g_set.outb_pwr);
    thermal_update();
}

void app_init(void)
{
    settings_load();
    lmx_init();                    /* register burst leaves chip briefly on */
    if (any_output_on()) {         /* restore ON state only if saved so */
        lmx_set_freq_khz(g_set.f_lo_khz);
        apply_outputs();
        if (g_set.sweep_mode) app_sweep(true);
    } else {
        lmx_powerdown(true);       /* default: everything OFF, chip cold (B1) */
        lmx_output_enable(LMX_OUTA, false);
        lmx_output_enable(LMX_OUTB, false);
        st = ST_IDLE_ALLOFF;
    }
}

void app_set_lo(uint32_t f_khz)
{
    g_set.f_lo_khz = f_khz;
    if (st != ST_IDLE_ALLOFF) lmx_set_freq_khz(f_khz);
    settings_mark_dirty(); ui_mark_dirty();
}

void app_output_enable(int which, bool on)
{
    if (which == 0) g_set.outa_en = on; else g_set.outb_en = on;
    apply_outputs();
    settings_mark_dirty(); ui_mark_dirty();
}

void app_set_power(int which, uint8_t pwr)
{
    if (which == 0) { g_set.outa_pwr = pwr; lmx_set_power(LMX_OUTA, pwr); }
    else            { g_set.outb_pwr = pwr; lmx_set_power(LMX_OUTB, pwr); }
    settings_mark_dirty(); ui_mark_dirty();
}

void app_set_f1(uint32_t f){ g_set.f1_khz = f; settings_mark_dirty(); }
void app_set_f2(uint32_t f){ g_set.f2_khz = f; settings_mark_dirty(); }
void app_set_step(uint32_t f){ g_set.step_khz = f; settings_mark_dirty(); }
void app_set_dwell(uint16_t ms){ g_set.dwell_ms = ms; settings_mark_dirty(); }

void app_sweep(bool run)
{
    g_set.sweep_mode = run;
    if (run && any_output_on()) {
        sweep_cur_khz = g_set.f1_khz;
        sweep_last_ms = 0;
        lmx_set_freq_khz(sweep_cur_khz);   /* B4: program first point immediately */
        st = ST_SWEEPING;
    } else {
        st = any_output_on() ? ST_ACTIVE : ST_IDLE_ALLOFF;
    }
    settings_save_now();   /* commit on start/stop */
}

void app_save(void){ settings_save_now(); }

/* superloop: non-blocking sweep advance + deferred settings commit. Fix B4. */
void app_tick(uint32_t now_ms)
{
    if (st == ST_SWEEPING) {
        if (g_set.step_khz && (now_ms - sweep_last_ms) >= g_set.dwell_ms) {
            sweep_last_ms = now_ms;
            sweep_cur_khz += g_set.step_khz;
            if (sweep_cur_khz > g_set.f2_khz) sweep_cur_khz = g_set.f1_khz;  /* wrap */
            lmx_set_freq_khz(sweep_cur_khz);
        }
    }
    settings_tick(now_ms);
}

void app_status(char *buf, uint32_t n)
{
    uint32_t f = (st == ST_SWEEPING) ? sweep_cur_khz : g_set.f_lo_khz;
    snprintf(buf, n, "LO:%lu A:%u B:%u PA:%u PB:%u L:%u SW:%u\r\n",
             (unsigned long)f, g_set.outa_en, g_set.outb_en,
             g_set.outa_pwr, g_set.outb_pwr, (unsigned)lmx_locked(), g_set.sweep_mode);
}
