#ifndef __UI_H
#define __UI_H

#include "stm32f1xx_hal.h"

/*
 * Menu / screen state machine driven by the four buttons.
 *
 *   MAIN        : 4 entries (Measure / Curve / Threshold / Control)
 *   MEASURE     : live sensor values
 *   CURVE_MENU  : 4 entries (Temperature / Humidity / Light / Waterlevel)
 *   CURVE_VIEW  : graph of the selected channel
 *   THRESHOLD   : 8 rows (T/H/L/W max & min), index 0..7
 *   CONTROL     : 3 rows (Fan / Light / Pump)
 */

void UI_Init(void);
void UI_Task(void);   /* call from the main loop: handle keys + redraw */
void UI_PushSample(float t, float h, float l, float w);

#endif /* __UI_H */
