/* ui.h — LCD menu UI driven by 5 buttons. */
#ifndef UI_H
#define UI_H
#include "buttons.h"

void ui_init(void);            /* splash + first paint */
void ui_handle(btn_t b);       /* process a button press */
void ui_refresh(void);         /* repaint dirty fields (call from superloop) */
void ui_mark_dirty(void);      /* force repaint (e.g. after CLI change) */

#endif /* UI_H */
