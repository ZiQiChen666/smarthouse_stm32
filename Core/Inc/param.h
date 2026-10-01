#ifndef __PARAM_H
#define __PARAM_H

#include "stm32f1xx_hal.h"

/*
 * 参数持久化（保存在片上 Flash 最后两页，掉电不丢失）
 *
 *  使用 Page62(0x0801F800) 和 Page63(0x0801FC00) 两页做「双页轮换」：
 *    - 每次保存，往序号较旧的那一页写一条带校验的记录
 *    - 记录只追加，不擦除；一页写满后擦掉另一页再继续
 *    - 上电时扫描两页，取序号最大且校验通过的那条恢复
 *  这样即使保存过程中掉电，也总有至少一条完整记录可用。
 *
 *  当前持久化的内容：4 组阈值的上下限（共 8 个 float）
 */

/* 保存的数据结构（阈值上下限） */
typedef struct {
    float temperatureMax;
    float temperatureMin;
    float humidityMax;
    float humidityMin;
    float lightMax;
    float lightMin;
    float waterlevelMax;
    float waterlevelMin;
} param_data_t;

/* 上电时调用：从 Flash 读回参数；若无有效记录则保持默认值
   返回 1 = 读到有效参数，0 = 没有（用默认值） */
uint8_t Param_Load(param_data_t *out);

/* 保存参数到 Flash（带校验，掉电安全）
   返回 0 = 成功，其它 = 失败 */
uint8_t Param_Save(const param_data_t *in);

/* 从全局变量直接读/写（封装成一步，简化调用） */
void    Param_LoadToVars(void);
uint8_t Param_SaveFromVars(void);

/* 调试用：打印参数区 Flash 原始内容 */
void Param_Dump(void);
void Param_DumpPage(uint32_t page_addr, const char *tag);

/* 自检：写一条测试记录再读回，验证 Flash 读写是否正常 */
uint8_t Param_SelfTest(void);

#endif /* __PARAM_H */
