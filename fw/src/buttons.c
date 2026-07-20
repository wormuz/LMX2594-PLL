/* buttons.c — 5 buttons active-low pull-up, 10ms debounce, edge-triggered.
 * Pins from board.h (verified reverse). Priority order matches stock. */
#include "buttons.h"
#include "board.h"

#define DEBOUNCE_MS 10u

static btn_t last_stable;
static btn_t candidate;
static unsigned cand_since;

static int pressed(GPIO_TypeDef *port, unsigned pin){ return (port->IDR & (1u << pin)) == 0; }

void buttons_init(void)
{
    /* GPIO configured as input pull-up in board init (gpio_init). Nothing per-pin here. */
    last_stable = BTN_NONE; candidate = BTN_NONE; cand_since = 0;
}

static btn_t raw_read(void)
{
    if (pressed(BTN_LEFT_PORT,   BTN_LEFT_PIN))   return BTN_LEFT;
    if (pressed(BTN_DOWN_PORT,   BTN_DOWN_PIN))   return BTN_DOWN;
    if (pressed(BTN_RIGHT_PORT,  BTN_RIGHT_PIN))  return BTN_RIGHT;
    if (pressed(BTN_UP_PORT,     BTN_UP_PIN))     return BTN_UP;
    if (pressed(BTN_CENTER_PORT, BTN_CENTER_PIN)) return BTN_CENTER;
    return BTN_NONE;
}

/* Returns a button code exactly once, on the debounced press edge. */
btn_t buttons_poll(unsigned now_ms)
{
    btn_t r = raw_read();
    if (r != candidate) { candidate = r; cand_since = now_ms; return BTN_NONE; }
    if ((now_ms - cand_since) < DEBOUNCE_MS) return BTN_NONE;
    if (candidate != last_stable) {          /* stable change */
        last_stable = candidate;
        if (candidate != BTN_NONE) return candidate;   /* press edge only */
    }
    return BTN_NONE;
}
