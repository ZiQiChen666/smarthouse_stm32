#include "button.h"
#include "tim.h"

#define BTN_PORT        GPIOB
#define BTN_DEBOUNCE_MS 20u
#define BTN_LONG_MS     700u
#define BTN_REPEAT_MS   500u

static const uint16_t s_pin[BTN_COUNT] = {
    GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15
};

typedef struct {
    uint8_t  raw;          /* raw logical state (1 = pressed) */
    volatile uint8_t stable;       /* debounced state                 */
    uint32_t last_change;  /* last raw change time            */
    uint32_t down_time;    /* time the stable press started   */
    uint8_t  long_fired;   /* long-press already reported     */
    uint32_t repeat_time;  /* next repeat time                */
    volatile button_event_t event; /* latched event                   */
    /* simulation */
    volatile uint8_t  sim_down;
    volatile uint32_t sim_release; /* 0 = no auto release             */
} btn_ctx_t;

static btn_ctx_t s_btn[BTN_COUNT];

void Button_Init(void)
{
    uint8_t i;
    for (i = 0; i < BTN_COUNT; i++) {
        s_btn[i].raw = 0;
        s_btn[i].stable = 0;
        s_btn[i].long_fired = 0;
        s_btn[i].event = BTN_EVENT_NONE;
        s_btn[i].sim_down = 0;
        s_btn[i].sim_release = 0;
    }

    /* TIM2 -> 10 ms periodic interrupt used to scan the keys */
    __HAL_TIM_SET_PRESCALER(&htim2, 7200u - 1u);   /* 72MHz/7200 = 10kHz */
    __HAL_TIM_SET_AUTORELOAD(&htim2, 100u - 1u);   /* 100 counts = 10ms  */
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    HAL_TIM_Base_Start_IT(&htim2);
}

static void btn_scan_one(uint8_t i, uint32_t now)
{
    btn_ctx_t *b = &s_btn[i];
    uint8_t physical;
    uint8_t down;

    /* simulated auto-release */
    if (b->sim_down && b->sim_release && (int32_t)(now - b->sim_release) >= 0) {
        b->sim_down = 0;
        b->sim_release = 0;
    }

    /* active low, internal pull-up */
    physical = (HAL_GPIO_ReadPin(BTN_PORT, s_pin[i]) == GPIO_PIN_RESET) ? 1u : 0u;
    down = (physical || b->sim_down) ? 1u : 0u;

    if (down != b->raw) {
        b->raw = down;
        b->last_change = now;
    }

    if ((now - b->last_change) >= BTN_DEBOUNCE_MS && b->stable != b->raw) {
        b->stable = b->raw;
        if (b->stable) {
            /* press edge */
            b->down_time = now;
            b->long_fired = 0;
            b->repeat_time = now + BTN_LONG_MS;
        } else {
            /* release edge: short press only if long never fired */
            if (!b->long_fired) b->event = BTN_EVENT_SHORT;
            b->long_fired = 0;
        }
    }

    if (b->stable) {
        if (!b->long_fired && (now - b->down_time) >= BTN_LONG_MS) {
            b->long_fired = 1;
            b->event = BTN_EVENT_LONG;
            b->repeat_time = now + BTN_REPEAT_MS;
        } else if (b->long_fired && (int32_t)(now - b->repeat_time) >= 0) {
            b->repeat_time = now + BTN_REPEAT_MS;
            b->event = BTN_EVENT_REPEAT;
        }
    }
}

void Button_Scan(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t i;
    for (i = 0; i < BTN_COUNT; i++) {
        btn_scan_one(i, now);
    }
}

button_event_t Button_GetEvent(button_id_t id)
{
    button_event_t e;
    if (id >= BTN_COUNT) return BTN_EVENT_NONE;
    e = s_btn[id].event;
    s_btn[id].event = BTN_EVENT_NONE;
    return e;
}

uint8_t Button_IsDown(button_id_t id)
{
    if (id >= BTN_COUNT) return 0;
    return s_btn[id].stable;
}

void Button_SimPress(button_id_t id, uint32_t ms)
{
    if (id >= BTN_COUNT) return;
    s_btn[id].sim_release = HAL_GetTick() + ms;
    s_btn[id].sim_down = 1;
}

void Button_SimSet(button_id_t id, uint8_t down)
{
    if (id >= BTN_COUNT) return;
    s_btn[id].sim_release = 0;
    s_btn[id].sim_down = down ? 1u : 0u;
}

/* TIM2 update interrupt -> key scan */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        Button_Scan();
    }
}
