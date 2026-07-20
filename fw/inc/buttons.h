/* buttons.h — 5 push buttons, debounced. Codes match stock FUN_0800266C. */
#ifndef BUTTONS_H
#define BUTTONS_H

typedef enum {
    BTN_NONE = 0, BTN_LEFT = 1, BTN_DOWN = 2, BTN_RIGHT = 3, BTN_UP = 4, BTN_CENTER = 5
} btn_t;

void  buttons_init(void);
btn_t buttons_poll(unsigned now_ms);   /* returns a button once per press (edge) */

#endif /* BUTTONS_H */
