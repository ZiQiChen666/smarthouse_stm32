#include "ui.h"
#include "oled.h"
#include "button.h"
#include "param.h"
#include <stdio.h>
#include <string.h>

/* variables owned by main.c */
extern float temperature, humidity, light, waterlevel;
extern float temperatureMax, temperatureMin;
extern float humidityMax, humidityMin;
extern float lightMax, lightMin;
extern float waterlevelMax, waterlevelMin;
extern float fanS, lightS, pumpS;

/* ==================================================================
 * 界面语言选择:  1 = 中文,  0 = 英文
 * ------------------------------------------------------------------
 * 中文依赖 Core/Src/hz16.c 字库, 且字库必须包含下面用到的所有汉字。
 * 当前中文界面用到的汉字(26 个):
 *   测量界面曲线阈值控制温度湿光照水位上下限风扇灯泵开关
 *
 * 中文串以 UTF-8 的 \xNN 转义保存(纯 ASCII 源码), 避免 Keil AC5
 * 按 GBK 解析源码时报 missing closing quote; 运行时字节仍是 UTF-8。
 * 行末注释是原中文, 仅供阅读。
 * ================================================================== */
#ifndef UI_LANG_CN
#define UI_LANG_CN  1
#endif

#if UI_LANG_CN

#define MI_MEASURE     "\xE6\xB5\x8B\xE9\x87\x8F\xE7\x95\x8C\xE9\x9D\xA2"  /* 测量界面 */
#define MI_CURVE       "\xE6\x9B\xB2\xE7\xBA\xBF\xE7\x95\x8C\xE9\x9D\xA2"  /* 曲线界面 */
#define MI_THRESHOLD   "\xE9\x98\x88\xE5\x80\xBC\xE7\x95\x8C\xE9\x9D\xA2"  /* 阈值界面 */
#define MI_CONTROL     "\xE6\x8E\xA7\xE5\x88\xB6\xE7\x95\x8C\xE9\x9D\xA2"  /* 控制界面 */

#define LBL_TEMP       "\xE6\xB8\xA9\xE5\xBA\xA6\xE6\xB5\x8B\xE9\x87\x8F:"  /* 温度测量: */
#define LBL_HUMI       "\xE6\xB9\xBF\xE5\xBA\xA6\xE6\xB5\x8B\xE9\x87\x8F:"  /* 湿度测量: */
#define LBL_LIGHT      "\xE5\x85\x89\xE7\x85\xA7\xE6\xB5\x8B\xE9\x87\x8F:"  /* 光照测量: */
#define LBL_WATER      "\xE6\xB0\xB4\xE4\xBD\x8D\xE6\xB5\x8B\xE9\x87\x8F:"  /* 水位测量: */

#define L_TEMP         "\xE6\xB8\xA9\xE5\xBA\xA6\xE6\x9B\xB2\xE7\xBA\xBF"  /* 温度曲线 */
#define L_HUMI         "\xE6\xB9\xBF\xE5\xBA\xA6\xE6\x9B\xB2\xE7\xBA\xBF"  /* 湿度曲线 */
#define L_LIGHT        "\xE5\x85\x89\xE7\x85\xA7\xE6\x9B\xB2\xE7\xBA\xBF"  /* 光照曲线 */
#define L_WATER        "\xE6\xB0\xB4\xE4\xBD\x8D\xE6\x9B\xB2\xE7\xBA\xBF"  /* 水位曲线 */

#define L_TMAX         "\xE6\xB8\xA9\xE5\xBA\xA6\xE4\xB8\x8A\xE9\x99\x90"  /* 温度上限 */
#define L_TMIN         "\xE6\xB8\xA9\xE5\xBA\xA6\xE4\xB8\x8B\xE9\x99\x90"  /* 温度下限 */
#define L_HMAX         "\xE6\xB9\xBF\xE5\xBA\xA6\xE4\xB8\x8A\xE9\x99\x90"  /* 湿度上限 */
#define L_HMIN         "\xE6\xB9\xBF\xE5\xBA\xA6\xE4\xB8\x8B\xE9\x99\x90"  /* 湿度下限 */
#define L_LMAX         "\xE5\x85\x89\xE7\x85\xA7\xE4\xB8\x8A\xE9\x99\x90"  /* 光照上限 */
#define L_LMIN         "\xE5\x85\x89\xE7\x85\xA7\xE4\xB8\x8B\xE9\x99\x90"  /* 光照下限 */
#define L_WMAX         "\xE6\xB0\xB4\xE4\xBD\x8D\xE4\xB8\x8A\xE9\x99\x90"  /* 水位上限 */
#define L_WMIN         "\xE6\xB0\xB4\xE4\xBD\x8D\xE4\xB8\x8B\xE9\x99\x90"  /* 水位下限 */

#define L_FAN          "\xE9\xA3\x8E\xE6\x89\x87\xE6\x8E\xA7\xE5\x88\xB6"  /* 风扇控制 */
#define L_LIGHTSW      "\xE7\x81\xAF\xE5\x85\x89\xE6\x8E\xA7\xE5\x88\xB6"  /* 灯光控制 */
#define L_PUMP         "\xE6\xB0\xB4\xE6\xB3\xB5\xE6\x8E\xA7\xE5\x88\xB6"  /* 水泵控制 */
#define L_ON           "\xE5\xBC\x80"  /* 开 */
#define L_OFF          "\xE5\x85\xB3"  /* 关 */

#define CURVE_TOP      16   /* 中文标题占 2 页(16px), 曲线图从 y=16 开始 */

#else  /* ---------------- 英文 ---------------- */

#define MI_MEASURE     "1.Measure"
#define MI_CURVE       "2.Curve"
#define MI_THRESHOLD   "3.Threshold"
#define MI_CONTROL     "4.Control"

#define LBL_TEMP       "TEMP :"
#define LBL_HUMI       "HUMI :"
#define LBL_LIGHT      "LIGHT:"
#define LBL_WATER      "WATER:"

#define L_TEMP         "Temperature"
#define L_HUMI         "Humidity"
#define L_LIGHT        "Light"
#define L_WATER        "Waterlevel"

#define L_TMAX         "T-MAX"
#define L_TMIN         "T-MIN"
#define L_HMAX         "H-MAX"
#define L_HMIN         "H-MIN"
#define L_LMAX         "L-MAX"
#define L_LMIN         "L-MIN"
#define L_WMAX         "W-MAX"
#define L_WMIN         "W-MIN"

#define L_FAN          "FAN"
#define L_LIGHTSW      "LIGHT"
#define L_PUMP         "PUMP"
#define L_ON           "ON"
#define L_OFF          "OFF"

#define CURVE_TOP      12

#endif

/* 左侧给 '>' 留 8 像素, 内容一律从 x=8 开始(有无光标都不跳动) */
#define UI_TEXT_X   8
#define UI_ROWS     4

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
static uint8_t     s_thresh_dirty = 0;   /* 阈值被改过，待写入本地 Flash */

/* 曲线历史：同时只画一条曲线，所以只需要一维缓冲。
 * 原来是 uint8_t[4][128] = 512 字节，改成 uint8_t[128] = 128 字节。
 * 只有进入曲线视图(SCREEN_CURVE_VIEW)时才往里压值。 */
static uint8_t s_hist[CURVE_LEN];
static uint8_t s_hist_cnt = 0;
static uint8_t s_hist_head = 0;

/* ---- 行缓存: 只刷新内容真正变化的行, 不整屏重复刷 ----
 * s_row_txt 保存上一次画到屏幕上的字符串, 每轮比较;
 * s_row_sel 保存上一次的选中状态(决定 x=0 处有没有 '>')。 */
static char        s_row_txt[UI_ROWS][28];
static uint8_t     s_row_sel[UI_ROWS];
static uint8_t     s_row_valid = 0;          /* 0 = 缓存失效, 全部重画 */
static uint8_t     s_page_shown = 0;         /* 阈值页右上角已显示的页码(0=无) */
static ui_screen_t s_last_screen = (ui_screen_t)0xFF;
static uint8_t     s_curve_dirty = 1;        /* 曲线视图需要重画 */

static const char *const s_main_items[4]  = { MI_MEASURE, MI_CURVE, MI_THRESHOLD, MI_CONTROL };
static const char *const s_curve_items[4] = { L_TEMP, L_HUMI, L_LIGHT, L_WATER };
static const char *const s_thresh_name[8] = { L_TMAX, L_TMIN, L_HMAX, L_HMIN,
                                              L_LMAX, L_LMIN, L_WMAX, L_WMIN };
static const char *const s_ctrl_name[3]   = { L_FAN, L_LIGHTSW, L_PUMP };

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
/* 画一行(16px 高, 占 2 页)。row = 0~3 -> page = 0,2,4,6。
 * 内容没变(文字+选中状态)则直接跳过, 不产生任何 I2C 流量。
 * 返回 1 表示这一行真的重画了。 */
static uint8_t ui_row(uint8_t row, const char *text, uint8_t selected)
{
    uint8_t page = (uint8_t)(row * 2u);

    if (s_row_valid && s_row_sel[row] == selected &&
        strncmp(s_row_txt[row], text, sizeof(s_row_txt[row])) == 0)
        return 0;

    strncpy(s_row_txt[row], text, sizeof(s_row_txt[row]) - 1);
    s_row_txt[row][sizeof(s_row_txt[row]) - 1] = '\0';
    s_row_sel[row] = selected;

    /* 只清/画这一行的 2 页, 其它行完全不动 */
    OLED_Clear_some(0, (uint8_t)(page * 8u), 128, (uint8_t)(page * 8u + 15u));
#if UI_LANG_CN
    OLED_ShowCNString(UI_TEXT_X, page, text);
#else
    OLED_ShowString(UI_TEXT_X, page, (const uint8_t *)text, 16);
#endif
    if (selected)
        OLED_ShowChar(0, page, '>', 16);

    return 1;
}

static void ui_invalidate(void)
{
    s_row_valid = 0;
    s_page_shown = 0;
}

/* ------------------------------------------------------------------ */
static void ui_draw_main(void)
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ui_row(i, s_main_items[i], (uint8_t)(s_index == i));
}

static void ui_draw_measure(void)
{
    char line[28];
    snprintf(line, sizeof(line), "%s%.1f", LBL_TEMP,  temperature); ui_row(0, line, 0);
    snprintf(line, sizeof(line), "%s%.1f", LBL_HUMI,  humidity);    ui_row(1, line, 0);
    snprintf(line, sizeof(line), "%s%.1f", LBL_LIGHT, light);       ui_row(2, line, 0);
    snprintf(line, sizeof(line), "%s%.1f", LBL_WATER, waterlevel);  ui_row(3, line, 0);
}

static void ui_draw_curve_menu(void)
{
    uint8_t i;
    for (i = 0; i < 4; i++)
        ui_row(i, s_curve_items[i], (uint8_t)(s_index == i));
}

static void ui_draw_threshold(void)
{
    char line[28];
    uint8_t i;
    uint8_t page = (uint8_t)(s_index / 4u);   /* index 0~3 -> page 0, 4~7 -> page 1 */
    uint8_t row0_drew = 0;

    for (i = 0; i < 4; i++) {
        uint8_t vi = (uint8_t)(page * 4u + i);
        snprintf(line, sizeof(line), "%s:%.0f", s_thresh_name[vi], *threshold_var(vi));
        if (i == 0) row0_drew = ui_row(0, line, (uint8_t)(s_index == vi));
        else        ui_row(i, line, (uint8_t)(s_index == vi));
    }

    /* 右上角页码。第 0 行重画时会把它擦掉, 所以只有需要时才补画 */
    if (row0_drew || s_page_shown != (uint8_t)(page + 1u)) {
        s_page_shown = (uint8_t)(page + 1u);
        snprintf(line, sizeof(line), "%d/2", page + 1);
        OLED_ShowString(104, 0, (const uint8_t *)line, 8);
    }
}

static void ui_draw_control(void)
{
    char line[28];
    uint8_t i;
    for (i = 0; i < 3; i++) {
#if UI_LANG_CN
        snprintf(line, sizeof(line), "%s:%s", s_ctrl_name[i],
                 (*control_var(i) >= 0.5f) ? L_ON : L_OFF);
#else
        snprintf(line, sizeof(line), "%-5s:%s", s_ctrl_name[i],
                 (*control_var(i) >= 0.5f) ? L_ON : L_OFF);
#endif
        ui_row(i, line, (uint8_t)(s_index == i));
    }
    ui_row(3, "", 0);      /* 第 4 行留空 */
}

static void ui_draw_curve(void)
{
    uint8_t cnt;
    uint8_t i;
    uint8_t prev_x = 0, prev_y = 0;

    if (!s_curve_dirty)
        return;                 /* 没有新采样点就不重画 */
    s_curve_dirty = 0;

    cnt = s_hist_cnt;           /* number of valid samples (<= 128) */
    OLED_Clear_Gram();

    /* title */
#if UI_LANG_CN
    OLED_DrawCNString(0, 0, s_curve_items[s_curve_type]);
#else
    {
        char title[24];
        snprintf(title, sizeof(title), "%s", s_curve_items[s_curve_type]);
        OLED_DrawString(0, 0, (const uint8_t *)title, 8);
    }
#endif
    OLED_DrawString(78, 0, (const uint8_t *)"0-100", 8);

    /* axes: y from CURVE_TOP..63 maps value 0..100 */
    OLED_Draw_Line(0, CURVE_TOP, 0, 63);
    OLED_Draw_Line(0, 63, 127, 63);

    /* one 200ms sample per X pixel, drawn left-to-right (oldest at x=0) */
    for (i = 0; i < cnt; i++) {
        uint8_t idx = (uint8_t)((s_hist_head + CURVE_LEN - cnt + i) % CURVE_LEN);
        uint8_t v = s_hist[idx];   /* 0~100 */
        uint8_t x, y;
        if (v > 100u) v = 100u;
        x = i;                                  /* left-aligned: oldest at left */
        y = (uint8_t)(63u - (uint16_t)v * (63u - CURVE_TOP) / 100u);
        if (i > 0)
            OLED_Draw_Line(prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
    OLED_Refresh();
}

static void ui_draw(void)
{
    /* 换屏: 清一次全屏并让行缓存失效, 之后只刷变化的行 */
    if (s_screen != s_last_screen) {
        s_last_screen = s_screen;
        ui_invalidate();
        s_curve_dirty = 1;
        if (s_screen != SCREEN_CURVE_VIEW)
            OLED_Clear();
    }

    switch (s_screen) {
    case SCREEN_MEASURE:    ui_draw_measure();    break;
    case SCREEN_CURVE_MENU: ui_draw_curve_menu(); break;
    case SCREEN_CURVE_VIEW: ui_draw_curve();      break;
    case SCREEN_THRESHOLD:  ui_draw_threshold();  break;
    case SCREEN_CONTROL:    ui_draw_control();    break;
    case SCREEN_MAIN:
    default:                ui_draw_main();       break;
    }

    s_row_valid = 1;   /* 本轮所有行都已对比/绘制, 之后即可用缓存跳过未变行 */
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
        /* 进入曲线视图：先清空历史，避免上一个通道的旧数据残留 */
        s_hist_cnt = 0;
        s_hist_head = 0;
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
        /* 离开阈值界面时，如果改过就写本地 Flash（掉电不丢）。
           不上报云端：阈值只以本地为准。 */
        if (s_screen == SCREEN_THRESHOLD && s_thresh_dirty) {
            if (Param_SaveFromVars() == 0)
                s_thresh_dirty = 0;
        }
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
            s_thresh_dirty = 1;     /* 标记改动，离开界面时写 Flash */
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
            s_thresh_dirty = 1;     /* 标记改动，离开界面时写 Flash */
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
    s_last_screen = (ui_screen_t)0xFF;
    s_row_valid = 0;
    s_page_shown = 0;
    s_curve_dirty = 1;
}

void UI_Task(void)
{
    button_event_t ev;

    ev = Button_GetEvent(BTN_PB14);
    if (ev == BTN_EVENT_SHORT) ui_on_confirm();

    ev = Button_GetEvent(BTN_PB15);
    if (ev == BTN_EVENT_SHORT) ui_on_back();

    ev = Button_GetEvent(BTN_PB12);
    if (ev != BTN_EVENT_NONE) ui_on_inc(ev);

    ev = Button_GetEvent(BTN_PB13);
    if (ev != BTN_EVENT_NONE) ui_on_dec(ev);

    /* 每轮都进 ui_draw(); 行缓存会保证只有内容变化的行才真正刷屏 */
    ui_draw();
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
    /* 只有进入曲线视图时才记录历史，且只记当前选中的那一路。
     * 其它界面下完全不占时间也不压值。 */
    if (s_screen == SCREEN_CURVE_VIEW)
    {
        float v;
        switch (s_curve_type) {
        case 0:  v = t; break;   /* Temperature */
        case 1:  v = h; break;   /* Humidity    */
        case 2:  v = l; break;   /* Light       */
        default: v = w; break;   /* Waterlevel  */
        }

        s_hist[s_hist_head] = ui_clamp_u8(v);   /* 0~100 */
        s_hist_head = (uint8_t)((s_hist_head + 1) % CURVE_LEN);
        if (s_hist_cnt < CURVE_LEN) s_hist_cnt++;

        s_curve_dirty = 1;       /* 有新采样点, 重画一次曲线 */
    }
    /* 测量界面不在这里置标志: UI_Task 每轮都会调用 ui_draw(),
       行缓存会自动只刷新数值变化的那几行。 */
}
