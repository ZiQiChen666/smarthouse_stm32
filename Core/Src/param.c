/**
  ******************************************************************************
  * @file    param.c
  * @brief   参数持久化（片上 Flash，双页轮换，掉电安全）
  *
  *  ---- 为什么用双页轮换 ----
  *  片上 Flash 只能「整页擦除 -> 写入」，不能在原处修改。
  *  如果只用一页：擦除后正在写时掉电 -> 数据全丢。
  *  双页轮换：写到另一页，旧页数据始终保留到新页写成功为止。
  *
  *  ---- 记录格式（每条 4 + N + 1 字节）----
  *      uint32_t  seq      记录序号（每次保存 +1）
  *      param_data_t data  参数内容
  *      uint8_t   checksum 前若干字节的异或校验
  *
  *  ---- 页内布局 ----
  *      全是 0xFF 表示空槽；写入后槽内不再是 0xFF。
  *      保存时：找到当前页第一个空槽写入；页满则换另一页（先擦除）。
  ******************************************************************************
  */

#include "param.h"
#include <string.h>
#include <stddef.h>

/* ---- Flash 页定义（F103 中容量，每页 1KB）---- */
#define PARAM_PAGE_A_ADDR    0x0801F800UL    /* Page 62 */
#define PARAM_PAGE_B_ADDR    0x0801FC00UL    /* Page 63 */
#define PARAM_PAGE_SIZE      0x400UL         /* 1KB */

/* 记录头/尾 */
#define REC_MAGIC            0x50415241UL    /* "PARA" 用于识别有效记录头 */

typedef struct {
    uint32_t     magic;      /* 固定标识，用于判断该槽是否已写 */
    uint32_t     seq;        /* 序号 */
    param_data_t data;       /* 参数 */
    uint8_t      checksum;   /* 校验 */
} param_rec_t;

#define REC_SIZE             ((uint16_t)sizeof(param_rec_t))
#define RECS_PER_PAGE        ((uint16_t)(PARAM_PAGE_SIZE / REC_SIZE))

/* ---- 全局变量（定义在 main.c）---- */
extern float temperatureMax, temperatureMin;
extern float humidityMax, humidityMin;
extern float lightMax, lightMin;
extern float waterlevelMax, waterlevelMin;

/* ------------------------------------------------------------------ */
/* 计算一条记录的校验和（除 checksum 外的所有字节异或）                */
/* ------------------------------------------------------------------ */
static uint8_t param_checksum(const param_rec_t *r)
{
    const uint8_t *p = (const uint8_t *)r;
    uint16_t i;
    uint8_t sum = 0;

    /* 只校验 checksum 字段之前的字节（magic + seq + data）。
       不能用 REC_SIZE-1：结构体尾部对齐填充会落在 checksum 之后，
       而 checksum 本身也可能不在最后一个字节上，导致写入/校验时
       算进的字节不一致 -> 校验永远失败。 */
    uint16_t n = (uint16_t)offsetof(param_rec_t, checksum);
    for (i = 0; i < n; i++) sum ^= p[i];
    return sum;
}

/* 校验一条记录是否有效 */
static uint8_t param_rec_valid(const param_rec_t *r)
{
    if (r->magic != REC_MAGIC) return 0;
    return (param_checksum(r) == r->checksum) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Flash 写：按半字（16bit）写入                                          */
/* ------------------------------------------------------------------ */
static uint8_t param_flash_write(uint32_t addr, const void *buf, uint16_t len)
{
    const uint8_t *bp = (const uint8_t *)buf;
    uint16_t i;

    HAL_FLASH_Unlock();

    /* 逐半字写入（F103 只支持半字/字编程）。
       结构体长度可能为奇数（41），末字节高半字补 0xFF（和擦除态一致）。 */
    for (i = 0; i < len; i += 2)
    {
        uint16_t hw = (uint16_t)bp[i];
        if ((i + 1) < len)
            hw |= (uint16_t)((uint16_t)bp[i + 1] << 8);
        else
            hw |= 0xFF00u;

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, hw) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 1;
        }
    }

    HAL_FLASH_Lock();
    return 0;
}

/* Flash 擦页 */
static uint8_t param_flash_erase(uint32_t page_addr)
{
    FLASH_EraseInitTypeDef e;
    uint32_t err = 0;

    e.TypeErase   = FLASH_TYPEERASE_PAGES;
    e.PageAddress = page_addr;
    e.NbPages     = 1;

    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&e, &err) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 1;
    }
    HAL_FLASH_Lock();
    return 0;
}

/* ------------------------------------------------------------------ */
/* 扫描一页：返回有效记录数，并输出序号最大的那条                       */
/* ------------------------------------------------------------------ */
static uint16_t param_scan_page(uint32_t page_addr, param_rec_t *best, uint32_t *best_seq)
{
    uint16_t i;
    uint16_t count = 0;
    uint32_t maxseq = 0;
    uint8_t  found = 0;

    for (i = 0; i < RECS_PER_PAGE; i++)
    {
        const param_rec_t *r = (const param_rec_t *)(page_addr + (uint32_t)i * REC_SIZE);

        /* 空槽：magic 全 1（0xFFFFFFFF）说明没写过 */
        if (r->magic == 0xFFFFFFFFUL) continue;

        if (param_rec_valid(r))
        {
            count++;
            if (!found || r->seq >= maxseq)   /* >= 处理序号回绕/相同 */
            {
                maxseq = r->seq;
                if (best) memcpy(best, r, REC_SIZE);
                found = 1;
            }
        }
    }

    if (best_seq) *best_seq = found ? maxseq : 0;
    return count;
}

/* 找一页里第一个空槽的序号，找不到返回 0xFFFF */
static uint16_t param_first_empty(uint32_t page_addr)
{
    uint16_t i;
    for (i = 0; i < RECS_PER_PAGE; i++)
    {
        const param_rec_t *r = (const param_rec_t *)(page_addr + (uint32_t)i * REC_SIZE);
        if (r->magic == 0xFFFFFFFFUL) return i;
    }
    return 0xFFFF;
}

/* ------------------------------------------------------------------ */
uint8_t Param_Load(param_data_t *out)
{
    param_rec_t bestA, bestB;
    uint32_t seqA = 0, seqB = 0;
    uint16_t cntA, cntB;
    uint8_t okA, okB;

    if (out == NULL) return 0;

    cntA = param_scan_page(PARAM_PAGE_A_ADDR, &bestA, &seqA);
    cntB = param_scan_page(PARAM_PAGE_B_ADDR, &bestB, &seqB);

    okA = (cntA > 0) ? 1 : 0;
    okB = (cntB > 0) ? 1 : 0;

    if (!okA && !okB) return 0;       /* 两页都空 -> 用默认值 */

    /* 取序号较大的那条 */
    if (okA && okB)
    {
        const param_rec_t *r = (seqA >= seqB) ? &bestA : &bestB;
        memcpy(out, &r->data, sizeof(param_data_t));
    }
    else if (okA)
    {
        memcpy(out, &bestA.data, sizeof(param_data_t));
    }
    else
    {
        memcpy(out, &bestB.data, sizeof(param_data_t));
    }

    return 1;
}

uint8_t Param_Save(const param_data_t *in)
{
    param_rec_t rec;
    param_rec_t bestA, bestB;
    uint32_t seqA = 0, seqB = 0;
    uint32_t next_seq;
    uint32_t page_addr;
    uint16_t slot;

    if (in == NULL) return 1;

    /* 找出当前最大序号（两页比较） */
    param_scan_page(PARAM_PAGE_A_ADDR, &bestA, &seqA);
    param_scan_page(PARAM_PAGE_B_ADDR, &bestB, &seqB);
    next_seq = (seqA > seqB ? seqA : seqB) + 1;

    /* 先尝试 A 页的空槽；没空槽就擦 A 页（此时 B 页仍有旧数据，安全） */
    page_addr = PARAM_PAGE_A_ADDR;
    slot = param_first_empty(page_addr);
    if (slot == 0xFFFF)
    {
        /* A 页满：改写到 B 页（若 B 页也满，则擦 B 页） */
        page_addr = PARAM_PAGE_B_ADDR;
        slot = param_first_empty(page_addr);
        if (slot == 0xFFFF)
        {
            if (param_flash_erase(page_addr) != 0) return 2;
            slot = 0;
        }
    }

    /* 组装记录（先清零，保证填充字节确定，校验才稳定） */
    memset(&rec, 0, sizeof(rec));
    rec.magic = REC_MAGIC;
    rec.seq   = next_seq;
    memcpy(&rec.data, in, sizeof(param_data_t));
    rec.checksum = param_checksum(&rec);

    /* 写入 */
    if (param_flash_write(page_addr + (uint32_t)slot * REC_SIZE, &rec, REC_SIZE) != 0)
        return 3;

    return 0;
}

/* ------------------------------------------------------------------ */
void Param_LoadToVars(void)
{
    param_data_t p;
    if (Param_Load(&p))
    {
        temperatureMax = p.temperatureMax;
        temperatureMin = p.temperatureMin;
        humidityMax    = p.humidityMax;
        humidityMin    = p.humidityMin;
        lightMax       = p.lightMax;
        lightMin       = p.lightMin;
        waterlevelMax  = p.waterlevelMax;
        waterlevelMin  = p.waterlevelMin;
    }
}

uint8_t Param_SaveFromVars(void)
{
    param_data_t p;
    p.temperatureMax = temperatureMax;
    p.temperatureMin = temperatureMin;
    p.humidityMax    = humidityMax;
    p.humidityMin    = humidityMin;
    p.lightMax       = lightMax;
    p.lightMin       = lightMin;
    p.waterlevelMax  = waterlevelMax;
    p.waterlevelMin  = waterlevelMin;
    return Param_Save(&p);
}

/* ------------------------------------------------------------------ */
/* 自检：写一个特值 -> 读回 -> 比对                                          */
/* ------------------------------------------------------------------ */
uint8_t Param_SelfTest(void)
{
    param_data_t w, r;
    uint8_t rc;
    const float T1 = 12.5f, T2 = 34.5f, T3 = 56.5f, T4 = 78.5f;

    w.temperatureMax = T1;
    w.temperatureMin = T2;
    w.humidityMax    = T3;
    w.humidityMin    = T4;
    w.lightMax       = 11.0f;
    w.lightMin       = 22.0f;
    w.waterlevelMax  = 33.0f;
    w.waterlevelMin  = 44.0f;

    rc = Param_Save(&w);
    if (rc != 0)
    {
        return (uint8_t)(0x10 + rc);   /* 0x11~ 表示写入失败 */
    }

    r = (param_data_t){0};
    if (Param_Load(&r) == 0)
        return 0x20;                    /* 读不到 */

    /* 逐个比对 */
    if (r.temperatureMax != T1) return 0x31;
    if (r.temperatureMin != T2) return 0x32;
    if (r.humidityMax    != T3) return 0x33;
    if (r.humidityMin    != T4) return 0x34;
    if (r.lightMax       != 11.0f) return 0x35;
    if (r.lightMin       != 22.0f) return 0x36;
    if (r.waterlevelMax  != 33.0f) return 0x37;
    if (r.waterlevelMin  != 44.0f) return 0x38;

    return 0;   /* 全部通过 */
}
