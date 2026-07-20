/* app.h — application layer: state machine, actions invoked by CLI/UI. */
#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { ST_ACTIVE, ST_IDLE_ALLOFF, ST_SWEEPING } app_state_t;

void app_init(void);
void app_tick(uint32_t now_ms);      /* superloop: sweep step + thermal + settings */

/* actions (called by CLI and UI) — all keep settings shadow + hardware in sync */
void app_set_lo(uint32_t f_khz);
void app_output_enable(int which /*0=A,1=B*/, bool on);
void app_set_power(int which, uint8_t pwr);
void app_set_f1(uint32_t f_khz);
void app_set_f2(uint32_t f_khz);
void app_set_step(uint32_t f_khz);
void app_set_dwell(uint16_t ms);
void app_sweep(bool run);
void app_save(void);
void app_status(char *buf, uint32_t n);  /* fills "LO:.. A:.. B:.. PA:.. PB:.. L:.. SW:.." */

#endif /* APP_H */
