#include "ui.h"
#include "oled.h"
#include "button.h"
#include <stdio.h>
#include <string.h>

/* variables owned by main.c */
extern float temperature, humidity, light, waterlevel;
extern float temperatureMax, temperatureMin;
extern float humidityMax, humidityMin;
extern float lightMax, lightMin;
extern float waterlevelMax, waterlevelMin;
extern float fanS, lightS, pumpS;

/*
 * Curve history.
 * ADC sampling (and therefore curve points) run every 200 ms, so one
 * sample per X pixel = 128 * 0.2 s = ~25.6 s of history on screen.
 */
#define CURVE_LEN   128

typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_MEASURE,
    SCREEN_CURVE_MENU,
    SCREEN_CURVE_VIEW,
    SCREEN_THRESHOLD,
    SCREEN_CONTROL
} ui_screen_t;

static ui_screen_t s_screen = SCREEN_MAIN;
static uint8_t     s_index = 0;
static uint8_t     s_curve_type = 0;
static uint8_t     s_need_redraw = 1;

/* curve history: [channel][sample], ring buffer */
/* 曲线历史：存 uint8_t（0~100），绘图精度足够（纵向只有 51 pixel）。
 * 原来是 float[4][128] = 2048 字节，改成 uint8_t[4][128] = 512 字节。 */
static uint8_t s_hist[4][CURVE_LEN];
static uint8_t s_hist_cnt = 0;
static uint8_t s_hist_head = 0;

static const char *const s_main_items[4]  = { "1.Measure", "2.Curve", "3.Threshold", "4.Control" };
static const char *const s_curve_items[4] = { "Temperature", "Humidity", "Light", "Waterlevel" };
static const char *const s_thresh_name[8] = { "T-MAX", "T-MIN", "H-MAX", "H-MIN",
                                              "L-MAX", "L-MIN", "W-MAX", "W-MIN" };
static const char *const s_ctrl_name[3]   = { "FAN", "LIGHT", "PUMP" };

/* ------------------------------------------------------------------ */
static float *threshold_var(uint8_t idx)
{
    switch (idx) {
    case 0: return &temperatureMax;
    case 1: return &temperatureMin;
    case 2: return &humidityMax;
    case 3: return &humidityMin;
    case 4: return &lightMax;
    case 5: return &lightMin;
    case 6: return &waterlevelMax;
    default: return &waterlevelMin;
    }
}

static float *control_var(uint8_t idx)
{
    switch (idx) {
    case 0: return &fanS;
    case 1: return &lightS;
    default: return &pumpS;
    }
}

/* ------------------------------------------------------------------ */
/* 16px tall line at page y (uses page y and y+1). 15 chars = 120px,  */
/* then clear the remaining right strip so no old pixels survive.      */
static void ui_label16(uint8_t page, const char *text, uint8_t selected)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%c%-14.14s", selected ? '>' : ' ', text);
    OLED_ShowString(0, page, (const uint8_t *)buf, 16);
}

/* clear the right-most columns that variable-width text does not cover */
static void ui_clear_right(void)
{
    OLED_Clear_some(120, 0, 128, 63);
}

/* ------------------------------------------------------------------ */
static void ui_draw_main(void)
{
    uint8_t i;
    ui_clear_right();
    for (i = 0; i < 4; i++)
        ui_label16((uint8_t)(i * 2), s_main_items[i], (uint8_t)(s_index == i));
}

static void ui_draw_measure(void)
{
    char line[24];
    ui_clear_right();
    snprintf(line, sizeof(line), "TEMP :%.1f", temperature); ui_label16(0, line, 0);
    snprintf(line, sizeof(line), "HUMI :%.1f", humidity);    ui_label16(2, line, 0);
    snprintf(line, sizeof(line), "LIGHT:%.1f", light);       ui_label16(4, line, 0);
    snprintf(line, sizeof(line), "WATER:%.1f", waterlevel);  ui_label16(6, line, 0);
}

static void ui_draw_curve_menu(void)
{
    uint8_t i;
    ui_clear_right();
    for (i = 0; i < 4; i++)
        ui_label16((uint8_t)(i * 2), s_curve_items[i], (uint8_t)(s_index == i));
}

static void ui_draw_threshold(void)
{
    char line[24];
    uint8_t i;
    uint8_t page = (uint8_t)(s_index / 4);   /* index 0~3 -> page 0, 4~7 -> page 1 */

    ui_clear_right();
    for (i = 0; i < 4; i++) {
        uint8_t vi = (uint8_t)(page * 4u + i);
        snprintf(line, sizeof(line), "%s:%.1f", s_thresh_name[vi], *threshold_var(vi));
        ui_label16((uint8_t)(i * 2), line, (uint8_t)(s_index == vi));
    }

    /* page indicator, top-right corner (size 8): "1/2" or "2/2" */
    snprintf(line, sizeof(line), "%d/2", page + 1);
    OLED_ShowString(104, 0, (const uint8_t *)line, 8);
}

static void ui_draw_control(void)
{
    char line[24];
    uint8_t i;
    ui_clear_right();
    for (i = 0; i < 3; i++) {
        snprintf(line, sizeof(line), "%-5s:%s", s_ctrl_name[i],
                 (*control_var(i) >= 0.5f) ? "ON" : "OFF");
        ui_label16((uint8_t)(i * 2), line, (uint8_t)(s_index == i));
    }
    ui_label16(6, "", 0);
}

static void ui_draw_curve(void)
{
    char title[24];
    uint8_t cnt = s_hist_cnt;   /* number of valid samples (<= 128) */
    uint8_t i;
    uint8_t prev_x = 0, prev_y = 0;

    OLED_Clear_Gram();

    /* title on page 0 (8px) */
    snprintf(title, sizeof(title), "%s", s_curve_items[s_curve_type]);
    OLED_DrawString(0, 0, (const uint8_t *)title, 8);
    OLED_DrawString(78, 0, (const uint8_t *)"0-100", 8);

    /* axes: y from 12..63 maps value 0..100 */
    OLED_Draw_Line(0, 12, 0, 63);
    OLED_Draw_Line(0, 63, 127, 63);

    /* one 200ms sample per X pixel, newest sample at x=127 */
    for (i = 0; i < cnt; i++) {
        uint8_t idx = (uint8_t)((s_hist_head + CURVE_LEN - cnt + i) % CURVE_LEN);
        uint8_t v = s_hist[s_curve_type][idx];   /* 0~100 */
        uint8_t x, y;
        if (v > 100u) v = 100u;
        x = (uint8_t)(127u - (cnt - 1u - i));   /* right-aligned */
        y = (uint8_t)(63u - (uint16_t)v * 51u / 100u);
        if (i > 0)
            OLED_Draw_Line(prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
    OLED_Refresh();
}

static void ui_draw(void)
{
    switch (s_screen) {
    case SCREEN_MEASURE:    ui_draw_measure();    break;
    case SCREEN_CURVE_MENU: ui_draw_curve_menu(); break;
    case SCREEN_CURVE_VIEW: ui_draw_curve();      break;
    case SCREEN_THRESHOLD:  ui_draw_threshold();  break;
    case SCREEN_CONTROL:    ui_draw_control();    break;
    case SCREEN_MAIN:
    default:                ui_draw_main();       break;
    }
}

/* ------------------------------------------------------------------ */
static uint8_t ui_on_confirm(void)
{
    switch (s_screen) {
    case SCREEN_MAIN:
        if (s_index == 0)      s_screen = SCREEN_MEASURE;
        else if (s_index == 1) s_screen = SCREEN_CURVE_MENU;
        else if (s_index == 2) s_screen = SCREEN_THRESHOLD;
        else                   s_screen = SCREEN_CONTROL;
        s_index = 0;
        return 1;
    case SCREEN_CURVE_MENU:
        s_curve_type = s_index;
        s_screen = SCREEN_CURVE_VIEW;
        return 1;
    case SCREEN_CURVE_VIEW:
        s_screen = SCREEN_CURVE_MENU;
        return 1;
    default:
        return 0;
    }
}

static uint8_t ui_on_back(void)
{
    if (s_screen == SCREEN_MAIN)
        return 0;

    if (s_screen == SCREEN_CURVE_VIEW)
        s_screen = SCREEN_CURVE_MENU;
    else {
        s_screen = SCREEN_MAIN;
        s_index = 0;
    }
    return 1;
}

static uint8_t ui_on_inc(button_event_t ev)
{
    switch (s_screen) {
    case SCREEN_MAIN:
    case SCREEN_CURVE_MENU:
        if (ev == BTN_EVENT_SHORT) { s_index = (uint8_t)((s_index + 1) % 4); return 1; }
        break;
    case SCREEN_THRESHOLD:
        if (ev == BTN_EVENT_SHORT) { s_index = (uint8_t)((s_index + 1) % 8); return 1; }
        if (ev == BTN_EVENT_LONG || ev == BTN_EVENT_REPEAT) {
            float *v = threshold_var(s_index);
            *v += 1.0f;
            if (*v > 100.0f) *v = 100.0f;
            return 1;
        }
        break;
    case SCREEN_CONTROL:
        if (ev == BTN_EVENT_SHORT) { s_index = (uint8_t)((s_index + 1) % 3); return 1; }
        if (ev == BTN_EVENT_LONG || ev == BTN_EVENT_REPEAT) {
            *control_var(s_index) = 1.0f;   /* open */
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

static uint8_t ui_on_dec(button_event_t ev)
{
    switch (s_screen) {
    case SCREEN_MAIN:
    case SCREEN_CURVE_MENU:
        if (ev == BTN_EVENT_SHORT) { s_index = (uint8_t)((s_index + 3) % 4); return 1; }
        break;
    case SCREEN_THRESHOLD:
        if (ev == BTN_EVENT_SHORT) { s_index = (uint8_t)((s_index + 7) % 8); return 1; }
        if (ev == BTN_EVENT_LONG || ev == BTN_EVENT_REPEAT) {
            float *v = threshold_var(s_index);
            *v -= 1.0f;
            if (*v < 0.0f) *v = 0.0f;
            return 1;
        }
        break;
    case SCREEN_CONTROL:
        if (ev == BTN_EVENT_SHORT) { s_index = (uint8_t)((s_index + 2) % 3); return 1; }
        if (ev == BTN_EVENT_LONG || ev == BTN_EVENT_REPEAT) {
            *control_var(s_index) = 0.0f;   /* close */
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
void UI_Init(void)
{
    s_screen = SCREEN_MAIN;
    s_index = 0;
    s_curve_type = 0;
    s_hist_cnt = 0;
    s_hist_head = 0;
    s_need_redraw = 1;
}

void UI_Task(void)
{
    button_event_t ev;
    uint8_t redraw = 0;

    ev = Button_GetEvent(BTN_PB14);
    if (ev == BTN_EVENT_SHORT) redraw |= ui_on_confirm();

    ev = Button_GetEvent(BTN_PB15);
    if (ev == BTN_EVENT_SHORT) redraw |= ui_on_back();

    ev = Button_GetEvent(BTN_PB12);
    if (ev != BTN_EVENT_NONE) redraw |= ui_on_inc(ev);

    ev = Button_GetEvent(BTN_PB13);
    if (ev != BTN_EVENT_NONE) redraw |= ui_on_dec(ev);

    if (redraw) s_need_redraw = 1;

    if (s_need_redraw) {
        s_need_redraw = 0;
        ui_draw();
    }
}

/* 浮点值限制到 0~100 并转成 uint8_t */
static uint8_t ui_clamp_u8(float v)
{
    if (v <= 0.0f)   return 0u;
    if (v >= 100.0f) return 100u;
    return (uint8_t)(v + 0.5f);
}

void UI_PushSample(float t, float h, float l, float w)
{
    /* 存成 0~100 的整数，节省 RAM */
    s_hist[0][s_hist_head] = ui_clamp_u8(t);
    s_hist[1][s_hist_head] = ui_clamp_u8(h);
    s_hist[2][s_hist_head] = ui_clamp_u8(l);
    s_hist[3][s_hist_head] = ui_clamp_u8(w);
    s_hist_head = (uint8_t)((s_hist_head + 1) % CURVE_LEN);
    if (s_hist_cnt < CURVE_LEN) s_hist_cnt++;

    if (s_screen == SCREEN_MEASURE || s_screen == SCREEN_CURVE_VIEW)
        s_need_redraw = 1;
}
