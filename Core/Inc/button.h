#ifndef __BUTTON_H
#define __BUTTON_H

#include "stm32f1xx_hal.h"

/*
 * Button abstraction for PB12~PB15.
 *   PB12 = index +   (increase / open actuator)
 *   PB13 = index -   (decrease / close actuator)
 *   PB14 = confirm / enter
 *   PB15 = back / return
 *
 * Input source is (physical pin) OR (simulated state injected from the
 * USB debug console).  Button_Scan() is called from the TIM2 update
 * interrupt every 10 ms, so press timing is independent of the main loop.
 */

typedef enum {
    BTN_PB12 = 0,
    BTN_PB13,
    BTN_PB14,
    BTN_PB15,
    BTN_COUNT
} button_id_t;

typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_SHORT,    /* released before the long-press time */
    BTN_EVENT_LONG,     /* just reached the long-press time   */
    BTN_EVENT_REPEAT    /* still held, repeat interval elapsed */
} button_event_t;

void Button_Init(void);
void Button_Scan(void);
button_event_t Button_GetEvent(button_id_t id);  /* read & clear latched event */
uint8_t Button_IsDown(button_id_t id);           /* effective pressed state    */

/* ---- simulation / debug injection ---- */
void Button_SimPress(button_id_t id, uint32_t ms); /* press, auto-release after ms */
void Button_SimSet(button_id_t id, uint8_t down);  /* set state, no auto-release   */
uint8_t Button_SimGet(button_id_t id);

#endif /* __BUTTON_H */
