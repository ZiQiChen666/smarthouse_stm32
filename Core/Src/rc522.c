/**
  ******************************************************************************
  * @file    rc522.c
  * @brief   RC522 RFID 读卡模块驱动（软件 SPI：PA15=CS, PA5=SCK, PA7=MOSI, PA6=MISO）
  *
  *  RC522 寄存器访问（SPI 帧格式）：
  *      第 1 字节：地址
  *           bit7   = 1 表示读，0 表示写
  *           bit6:1 = 寄存器地址（6 位）
  *           bit0   = 0（读写时固定 0）
  *      第 2 字节起：数据（可连续多字节，地址自动递增）
  *
  *  软件 SPI：MSB first，CPOL=0 / CPHA=0，
  *      SCK 空闲低，数据在 SCK 上升沿被 RC522 采样（主机先摆好 MOSI 再拉高 SCK），
  *      MISO 在 SCK 下降沿之后读取。
  ******************************************************************************
  */

#include "rc522.h"

/* ---------------- 寄存器地址 ---------------- */
#define RC522_REG_COMMAND       0x01
#define RC522_REG_COM_I_EN      0x02
#define RC522_REG_DIV_I_EN      0x03
#define RC522_REG_COM_IRQ       0x04
#define RC522_REG_DIV_IRQ       0x05
#define RC522_REG_ERROR         0x06
#define RC522_REG_STATUS2       0x08
#define RC522_REG_FIFO_DATA     0x09
#define RC522_REG_FIFO_LEVEL    0x0A
#define RC522_REG_CONTROL       0x0C
#define RC522_REG_BIT_FRAMING   0x0D
#define RC522_REG_COLL          0x0E
#define RC522_REG_MODE          0x11
#define RC522_REG_TX_CONTROL    0x14
#define RC522_REG_TX_ASK        0x15
#define RC522_REG_CRC_RESULT_H  0x21
#define RC522_REG_CRC_RESULT_L  0x22
#define RC522_REG_T_MODE        0x2A
#define RC522_REG_T_PRESCALER   0x2B
#define RC522_REG_T_RELOAD_H    0x2C
#define RC522_REG_T_RELOAD_L    0x2D
#define RC522_REG_VERSION       0x37

/* ---------------- PICC 命令 ---------------- */
#define PICC_REQIDL             0x26    /* 寻天线区内未进入休眠的卡 */
#define PICC_REQALL             0x52    /* 寻所有卡 */
#define PICC_ANTICOLL           0x93    /* 防冲突 */
#define PICC_HALT               0x50    /* 休眠 */

/* ---------------- Mifare 通讯命令 ---------------- */
#define MF_AUTHENT               0x0E
#define MF_READ                  0x30
#define MF_WRITE                 0xA0
#define MF_TRANSCEIVE            0x0C
#define MF_RESET                 0x0F

/* 等待时间（循环计数），无卡时会超时退出 */
#define RC522_TIMEOUT           2000u

/* ------------------------------------------------------------------ */
/* 软件 SPI 底层                                                       */
/* ------------------------------------------------------------------ */
static void rc522_delay(void)
{
    /* 软件 SPI 不需要很快；几个 NOP 保证时序稳定（对应 ~几百 kHz） */
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
}

static void rc522_cs_low(void)  { HAL_GPIO_WritePin(RC522_CS_PORT,  RC522_CS_PIN,  GPIO_PIN_RESET); }
static void rc522_cs_high(void) { HAL_GPIO_WritePin(RC522_CS_PORT,  RC522_CS_PIN,  GPIO_PIN_SET); }
static void rc522_sck_low(void) { HAL_GPIO_WritePin(RC522_SCK_PORT, RC522_SCK_PIN, GPIO_PIN_RESET); }
static void rc522_sck_high(void){ HAL_GPIO_WritePin(RC522_SCK_PORT, RC522_SCK_PIN, GPIO_PIN_SET); }

static void rc522_mosi_set(uint8_t b)
{
    HAL_GPIO_WritePin(RC522_MOSI_PORT, RC522_MOSI_PIN, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint8_t rc522_miso_get(void)
{
    return (HAL_GPIO_ReadPin(RC522_MISO_PORT, RC522_MISO_PIN) == GPIO_PIN_SET) ? 1u : 0u;
}

/* 全双工收发一个字节：同时发出 tx，并回读 rx */
static uint8_t rc522_spi_xfer(uint8_t tx)
{
    uint8_t rx = 0;
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        /* MSB first */
        rc522_mosi_set((tx & 0x80u) ? 1u : 0u);
        tx = (uint8_t)(tx << 1);
        rc522_delay();

        rc522_sck_high();
        rc522_delay();

        rx = (uint8_t)(rx << 1);
        if (rc522_miso_get()) rx |= 0x01u;

        rc522_sck_low();
        rc522_delay();
    }
    return rx;
}

/* 只发不收 */
static void rc522_spi_write(uint8_t tx)
{
    (void)rc522_spi_xfer(tx);
}

/* ------------------------------------------------------------------ */
/* 寄存器读写（地址：bit7=1 读 / 0 写，bit0 固定 0）                    */
/* ------------------------------------------------------------------ */
static void rc522_write_reg(uint8_t addr, uint8_t val)
{
    rc522_cs_low();
    rc522_spi_write((uint8_t)((addr << 1) & 0x7E));
    rc522_spi_write(val);
    rc522_cs_high();
}

static uint8_t rc522_read_reg(uint8_t addr)
{
    uint8_t v;
    rc522_cs_low();
    rc522_spi_write((uint8_t)(((addr << 1) & 0x7E) | 0x80));
    v = rc522_spi_xfer(0x00);
    rc522_cs_high();
    return v;
}

/* 连续写多个寄存器（保留备用，当前未使用） */
#if 0
static void rc522_write_regs(uint8_t addr, const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    rc522_cs_low();
    rc522_spi_write((uint8_t)((addr << 1) & 0x7E));
    for (i = 0; i < len; i++) rc522_spi_write(buf[i]);
    rc522_cs_high();
}
#endif

static void rc522_set_bits(uint8_t addr, uint8_t mask)
{
    uint8_t v = rc522_read_reg(addr);
    rc522_write_reg(addr, (uint8_t)(v | mask));
}

static void rc522_clear_bits(uint8_t addr, uint8_t mask)
{
    uint8_t v = rc522_read_reg(addr);
    rc522_write_reg(addr, (uint8_t)(v & (uint8_t)(~mask)));
}

/* ------------------------------------------------------------------ */
/* 天线与收发                                                          */
/* ------------------------------------------------------------------ */
static void rc522_antenna_on(void)
{
    uint8_t v = rc522_read_reg(RC522_REG_TX_CONTROL);
    if (!(v & 0x03u))
        rc522_set_bits(RC522_REG_TX_CONTROL, 0x03);
}

static void rc522_reset(void)
{
    rc522_write_reg(RC522_REG_COMMAND, MF_RESET);
}

/* 与卡片通讯：发命令 cmd，发数据 send，收数据 back（可 NULL），back_len 最多收多少 */
static uint8_t rc522_to_card(uint8_t cmd, const uint8_t *send, uint8_t send_len,
                             uint8_t *back, uint16_t *back_len)
{
    uint8_t irq_en = 0;
    uint8_t wait_irq = 0;
    uint8_t irq;
    uint16_t i;
    uint8_t last_bits;
    uint8_t n;

    if (cmd == MF_AUTHENT)
    {
        irq_en   = 0x12;      /* ErrIRq | IdleIRq */
        wait_irq = 0x10;      /* IdleIRq */
    }
    else if (cmd == MF_TRANSCEIVE)
    {
        irq_en   = 0x77;      /* RxIRq | TxIRq | IdleIRq | ErrIRq | TimerIRq */
        wait_irq = 0x30;      /* RxIRq | IdleIRq */
    }
    else
    {
        return 1;
    }

    rc522_write_reg(RC522_REG_COM_I_EN, (uint8_t)(irq_en | 0x80));   /* 允许 IRQ */
    /* 关键：必须清掉 ComIrqReg 的全部中断标志（写 0x7F，Set1 位自动保持）。
       原实现只清了 bit7，上一次超时留下的 TimerIRq(0x01) 会让本次
       等待循环立刻命中，导致有卡也被判成超时/错误。 */
    rc522_write_reg(RC522_REG_COM_IRQ, 0x7Fu);
    rc522_set_bits(RC522_REG_FIFO_LEVEL, 0x80);                      /* 清 FIFO */
    rc522_write_reg(RC522_REG_COMMAND, 0x00);                        /* 空闲 */

    if (send && send_len)
    {
        uint8_t i2;
        for (i2 = 0; i2 < send_len; i2++)
            rc522_write_reg(RC522_REG_FIFO_DATA, send[i2]);
    }

    rc522_write_reg(RC522_REG_COMMAND, cmd);
    if (cmd == MF_TRANSCEIVE)
        rc522_set_bits(RC522_REG_BIT_FRAMING, 0x80);   /* StartSend */

    /* 等中断或超时。注意：读 ComIrqReg 会清标志，所以每轮把 irq 变量更新即可。 */
    i = RC522_TIMEOUT;
    do {
        irq = rc522_read_reg(RC522_REG_COM_IRQ);
        i--;
    } while ((i != 0) && !(irq & 0x01) && !(irq & wait_irq));

    rc522_clear_bits(RC522_REG_BIT_FRAMING, 0x80);     /* 停发送 */

    if (i == 0)
        return 2;                       /* 超时：无卡 */

    /* 定时器到期（TimerIRq）表示卡片没有响应 */
    if (irq & 0x01)
        return 2;

    /* 有错误标志就不能算成功 */
    if (rc522_read_reg(RC522_REG_ERROR) & 0x1Bu)
        return 3;

    if (!(irq & wait_irq))
        return 3;                       /* 出错 */

    /* 取 FIFO 里的数据 */
    n = rc522_read_reg(RC522_REG_FIFO_LEVEL);
    last_bits = rc522_read_reg(RC522_REG_CONTROL);
    last_bits = (uint8_t)(last_bits & 0x07u);

    if (back && back_len)
    {
        if (n > *back_len) n = (uint8_t)*back_len;
        for (i = 0; i < n; i++)
            back[i] = rc522_read_reg(RC522_REG_FIFO_DATA);
        *back_len = n;
    }

    (void)last_bits;
    return 0;
}

/* ------------------------------------------------------------------ */
uint8_t RC522_Init(void)
{
    GPIO_InitTypeDef g = {0};
    uint8_t ver;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* CS / SCK / MOSI 推挽输出；MISO 输入 */
    g.Pin   = RC522_CS_PIN | RC522_SCK_PIN | RC522_MOSI_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin  = RC522_MISO_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    rc522_cs_high();
    rc522_sck_low();
    rc522_mosi_set(0);

    HAL_Delay(50);

    rc522_reset();
    HAL_Delay(50);

    /* 定时器：内部定时，自动（TPrescaler 约 40kHz） */
    rc522_write_reg(RC522_REG_T_MODE, 0x8D);
    rc522_write_reg(RC522_REG_T_PRESCALER, 0x3E);
    rc522_write_reg(RC522_REG_T_RELOAD_H, 30);
    rc522_write_reg(RC522_REG_T_RELOAD_L, 0);

    /* 其他配置 */
    rc522_write_reg(RC522_REG_TX_ASK, 0x40);
    rc522_write_reg(RC522_REG_MODE, 0x3D);
    rc522_set_bits(RC522_REG_TX_CONTROL, 0x03);
    rc522_antenna_on();

    /* 清一次中断标志，避免上电残留影响后续寻卡 */
    rc522_write_reg(RC522_REG_COM_IRQ, 0x7Fu);

    /* 读版本寄存器：正常应为 0x91 / 0x92 / 0x12 等非 0x00/0xFF 值 */
    ver = rc522_read_reg(RC522_REG_VERSION);
    if (ver == 0x00 || ver == 0xFF)
        return 1;      /* 通讯失败（未接 / 接线错） */

    return 0;
}

/* 寻卡 */
uint8_t RC522_RequestCard(rc522_card_t *card)
{
    uint8_t buf[2] = { PICC_REQIDL, 0x00 };
    uint8_t back[2] = {0, 0};
    uint16_t back_len = sizeof(back);
    uint8_t rc;

    rc522_clear_bits(RC522_REG_COLL, 0x80);   /* 清冲突标志 */
    rc522_write_reg(RC522_REG_BIT_FRAMING, 0x07);   /* 7bit 短帧 */

    rc = rc522_to_card(MF_TRANSCEIVE, buf, 1, back, &back_len);
    if (rc != 0) return rc;

    if (back_len < 2) return 4;

    if (card) card->sak = back[0];
    return 0;
}

/* 防冲突，读 UID（4 字节简版；7/10 字节需循环，这里处理最常见的 4 字节） */
uint8_t RC522_Anticoll(rc522_card_t *card)
{
    uint8_t buf[2] = { PICC_ANTICOLL, 0x20 };
    uint8_t back[5] = {0, 0, 0, 0, 0};
    uint16_t back_len = sizeof(back);
    uint8_t rc;
    uint8_t i, chk;

    /* 防冲突：发送 0x93 + 0x20(完整 2 字节)，接收 5 字节(UID4+BCC) */
    rc522_write_reg(RC522_REG_BIT_FRAMING, 0x00);   /* 8bit 完整帧 */

    rc = rc522_to_card(MF_TRANSCEIVE, buf, 2, back, &back_len);
    if (rc != 0) return rc;

    if (back_len < 5) return 5;

    /* 前 4 字节是 UID，第 5 字节是 BCC（异或校验） */
    chk = 0;
    for (i = 0; i < 4; i++) chk ^= back[i];
    if (chk != back[4]) return 6;

    if (card)
    {
        for (i = 0; i < 4; i++) card->uid[i] = back[i];
        card->uid_len = 4;
        card->present = 1;
        card->last_seen_tick = HAL_GetTick();
    }
    return 0;
}

/* 一体化寻卡 + 读 UID */
uint8_t RC522_ReadCard(rc522_card_t *card)
{
    uint8_t rc;

    if (card == NULL) return 1;
    card->present = 0;

    rc = RC522_RequestCard(card);
    if (rc != 0) return rc;

    rc = RC522_Anticoll(card);
    if (rc != 0) return rc;

    return 0;
}

void RC522_Halt(void)
{
    uint8_t buf[4] = { PICC_HALT, 0x00, 0x00, 0x00 };
    uint8_t back[2];
    uint16_t back_len = sizeof(back);

    rc522_write_reg(RC522_REG_BIT_FRAMING, 0x00);
    rc522_to_card(MF_TRANSCEIVE, buf, 2, back, &back_len);
}
