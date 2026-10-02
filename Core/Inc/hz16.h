#ifndef __HZ16_H
#define __HZ16_H

/*
 * 16x16 汉字点阵字库 (由 tools/gen_hanzi.py 自动生成)
 *
 * 取模方式: 阴码 / 逐列式 / 顺向  (对应 PCtoLCD2002: 阴码+逐列式+顺向, C51格式)
 * 每字 32 字节:
 *   [0..15]  上半页(第 0~7 行), 每列一字节
 *   [16..31] 下半页(第 8~15 行), 每列一字节
 *   每字节 bit0 = 该页最上面一行, 1 = 点亮
 *
 * 与工程内 8x16 ASCII 字库 (F8X16) 的排列完全一致。
 *
 * 用法:
 *   int i = HZ16_Index("主");        // 取下标, 找不到返回 -1
 *   const unsigned char *dot = HZ16[i];   // 32 字节点阵
 */

extern const unsigned short HZ16_COUNT;
extern const unsigned short HZ16_CODE[];       /* 每个字的 Unicode 码点, 与 HZ16 一一对应 */
extern const unsigned char HZ16[][32];

/* 取字形下标; 传入一个 UTF-8 汉字的首字节指针; 找不到返回 -1 */
int HZ16_Index(const char *utf8);

#endif /* __HZ16_H */
