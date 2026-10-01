/**
  ******************************************************************************
  * @file    gps.c
  * @brief   GY-NEO6MV2 (NEO-6M) GPS 模块驱动，接 USART3_RX (PB11)
  *
  *  数据流：
  *    USART3 收到字节 -> GPS_FeedByte() -> 拼 NMEA 整句 -> 解析 RMC/GGA
  *
  *  NMEA 句子格式（以 $GPRMC 为例）：
  *    $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,,*hh
  *      字段0: $GPRMC
  *      字段1: UTC 时间 hhmmss.ss
  *      字段2: 状态 A=有效 V=无效
  *      字段3: 纬度 ddmm.mmmm
  *      字段4: 南北 N/S
  *      字段5: 经度 dddmm.mmmm
  *      字段6: 东西 E/W
  *      字段7: 地面速度（节）
  *      字段8: 航向
  *
  *    $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,q,ss,h.h,aa.a,M,...
  *      字段6: 定位质量  字段7: 卫星数  字段9: 海拔
  *
  *  解析在中断里完成，但不做任何耗时的字符串格式化/打印，
  *  只把结果写成定点整数存到结构体，供主循环上报。
  ******************************************************************************
  */

#include "gps.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* NMEA 接收缓冲（一行最长按 100 字节留）                              */
/* ------------------------------------------------------------------ */
#define GPS_LINE_MAX    100

static char           s_line[GPS_LINE_MAX];
static uint8_t        s_len = 0;
static volatile uint8_t s_line_ready = 0;   /* 收到完整一行，待主循环取用? */

/* 解析结果（在中断里写，主循环读） */
static volatile gps_data_t s_gps;

/* ------------------------------------------------------------------ */
/* 小工具：无 atof，自己把 "ddmm.mmmm" 转成 1e-6 度                  */
/* ------------------------------------------------------------------ */
/* 解析经纬度 "dddmm.mmmm" -> 放大 1e6 的度                      */
static int32_t gps_parse_coord(const char *s)
{
    /* 拆分整数部分和小数部分，避免浮点 */
    int32_t deg = 0;          /* 度 */
    int32_t min_int = 0;      /* 分的整数部分 */
    int32_t min_frac = 0;     /* 分的小数部分（放大 1e6） */
    int32_t frac_digits = 0;
    int32_t int_part = 0;
    uint8_t seen_dot = 0;

    if (s == NULL || *s == '\0') return 0;

    /* 先取出整数部分，如 "3112" 或 "12138" */
    while (*s >= '0' && *s <= '9') {
        int_part = int_part * 10 + (*s - '0');
        s++;
    }

    /* 小数点后是「分」的小数部分 */
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            if (frac_digits < 6) {
                min_frac = min_frac * 10 + (*s - '0');
                frac_digits++;
            }
            s++;
        }
        seen_dot = 1;
    }
    (void)seen_dot;

    /* 度 = 整数部分 / 100；分 = 整数部分 % 100 + 小数 */
    deg      = int_part / 100;
    min_int  = int_part % 100;

    /* 把分的小数部分补齐到 6 位 */
    while (frac_digits < 6) { min_frac *= 10; frac_digits++; }

    /* 结果(1e-6 度) = 度*1e6 + 分*1e6/60 + 分小数/60 */
    return deg * GPS_COORD_SCALE
         + (min_int * GPS_COORD_SCALE) / 60
         + min_frac / 60;
}

/* 解析 "hhmmss.ss" -> hh/mm/ss */
static void gps_parse_time(const char *s, uint8_t *hh, uint8_t *mm, uint8_t *ss)
{
    if (hh) *hh = 0;
    if (mm) *mm = 0;
    if (ss) *ss = 0;
    if (s == NULL) return;

    if (s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9' && hh)
        *hh = (uint8_t)((s[0]-'0')*10 + (s[1]-'0'));
    if (s[2] >= '0' && s[2] <= '9' && s[3] >= '0' && s[3] <= '9' && mm)
        *mm = (uint8_t)((s[2]-'0')*10 + (s[3]-'0'));
    if (s[4] >= '0' && s[4] <= '9' && s[5] >= '0' && s[5] <= '9' && ss)
        *ss = (uint8_t)((s[4]-'0')*10 + (s[5]-'0'));
}

/* 解析 "ddmmyy" -> day/month/year（两位年） */
static void gps_parse_date(const char *s, uint8_t *dd, uint8_t *mm, uint8_t *yy)
{
    if (dd) *dd = 0;
    if (mm) *mm = 0;
    if (yy) *yy = 0;
    if (s == NULL) return;

    if (s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9' && dd)
        *dd = (uint8_t)((s[0]-'0')*10 + (s[1]-'0'));
    if (s[2] >= '0' && s[2] <= '9' && s[3] >= '0' && s[3] <= '9' && mm)
        *mm = (uint8_t)((s[2]-'0')*10 + (s[3]-'0'));
    if (s[4] >= '0' && s[4] <= '9' && s[5] >= '0' && s[5] <= '9' && yy)
        *yy = (uint8_t)((s[4]-'0')*10 + (s[5]-'0'));
}

/* 简单无符号整数解析（只处理整数部分） */
static uint32_t gps_parse_u32(const char *s)
{
    uint32_t v = 0;
    if (s == NULL) return 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t)(*s - '0'); s++; }
    return v;
}

/* 解析 "x.x" -> 放大 10 倍 */
static int32_t gps_parse_x10(const char *s)
{
    int neg = 0;
    int32_t v = 0;
    if (s == NULL) return 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    if (*s == '.') {
        s++;
        if (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); }
        else v *= 10;
    } else {
        v *= 10;
    }
    return neg ? -v : v;
}

/* 解析 "x.xx" -> 放大 100 倍 */
static uint32_t gps_parse_u_x100(const char *s)
{
    uint32_t v = 0;
    if (s == NULL) return 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t)(*s - '0'); s++; }
    if (*s == '.') {
        s++;
        { uint8_t d = 0;
          while (*s >= '0' && *s <= '9' && d < 2) { v = v * 10 + (uint32_t)(*s - '0'); s++; d++; }
          while (d < 2) { v *= 10; d++; }
        }
    } else {
        v *= 100;
    }
    return v;
}

/* ------------------------------------------------------------------ */
/* 把一行 NMEA 按逗号拆字段，返回字段数和指针数组                      */
/* ------------------------------------------------------------------ */
#define GPS_MAX_FIELDS  20

static uint8_t gps_split(char *line, char *fields[], uint8_t maxf)
{
    uint8_t n = 0;
    char *p = line;
    if (*p == '$' || *p == '!') p++;   /* 跳过起始符 */

    while (n < maxf) {
        fields[n++] = p;
        while (*p && *p != ',' && *p != '*') p++;
        if (*p == '\0') break;
        if (*p == '*') { *p = '\0'; break; }   /* 校验和开始，结束 */
        *p = '\0';
        p++;
    }
    return n;
}

/* 解析一整行 */
static void gps_parse_line(char *line)
{
    char *f[GPS_MAX_FIELDS];
    uint8_t n;

    /* 只处理 $..RMC 和 $..GGA（兼容 GP/GN 前缀） */
    if (line[0] != '$') return;

    n = gps_split(line, f, GPS_MAX_FIELDS);
    if (n < 2) return;

    /* 判断句子类型：末 3 个字符 */
    {
        uint8_t len = (uint8_t)strlen(f[0]);
        const char *type;

        if (len < 3) return;
        type = f[0] + len - 3;

        if (strcmp(type, "RMC") == 0 && n >= 9) {
            /* f1=time f2=status f3=lat f4=N/S f5=lon f6=E/W f7=speed f9=date */
            uint8_t hh, mm, ss;
            uint8_t dd, mo, yy;
            gps_parse_time(f[1], &hh, &mm, &ss);
            gps_parse_date(f[9], &dd, &mo, &yy);

            if (f[2][0] == 'A') {
                int32_t lat = gps_parse_coord(f[3]);
                int32_t lon = gps_parse_coord(f[5]);
                if (f[4][0] == 'S') lat = -lat;
                if (f[6][0] == 'W') lon = -lon;

                s_gps.latitude  = lat;
                s_gps.longitude = lon;
                s_gps.valid     = 1;
                s_gps.updated   = 1;
            } else {
                s_gps.valid = 0;
            }

            s_gps.hour   = hh;
            s_gps.minute = mm;
            s_gps.second = ss;
            s_gps.day    = dd;
            s_gps.month  = mo;
            s_gps.year   = yy;
            s_gps.speed_cs = (uint16_t)gps_parse_u_x100(f[7]);
        }
        else if (strcmp(type, "GGA") == 0 && n >= 10) {
            /* f6=quality f7=sats f9=altitude */
            s_gps.fix_quality = (uint8_t)gps_parse_u32(f[6]);
            s_gps.sats        = (uint8_t)gps_parse_u32(f[7]);
            s_gps.altitude_dm = (int16_t)gps_parse_x10(f[9]);
        }
    }
}

/* ------------------------------------------------------------------ */
void GPS_Init(void)
{
    memset((void *)&s_gps, 0, sizeof(s_gps));
    s_len = 0;
    s_line_ready = 0;
}

void GPS_FeedByte(uint8_t b)
{
    /* 只在句首 '$' 开始收，遇到换行结束 */
    if (b == '$') {
        s_len = 0;
        s_line[s_len++] = '$';
        return;
    }

    if (s_len == 0) return;          /* 还没收到句首 */

    if (b == '\r' || b == '\n') {
        if (s_len > 6) {
            s_line[s_len] = '\0';
            gps_parse_line(s_line);
        }
        s_len = 0;
        return;
    }

    if (s_len < GPS_LINE_MAX - 1) {
        s_line[s_len++] = (char)b;
    } else {
        s_len = 0;                   /* 超长，丢弃 */
    }
}

void GPS_GetData(gps_data_t *out)
{
    uint8_t i;
    const volatile uint8_t *src;
    uint8_t *dst;

    if (out == NULL) return;

    /* 简单拷贝（结构体不大，中断里写的都是整型） */
    src = (const volatile uint8_t *)&s_gps;
    dst = (uint8_t *)out;
    for (i = 0; i < sizeof(s_gps); i++) dst[i] = src[i];
}

/**
  * @brief  把放大 1e6 的经纬度格式化成 "度.分" 字符串
  * @param  scaled: 经纬度 * 1e6（可能为负）
  */
void GPS_FormatCoord(int32_t scaled, char *buf, uint8_t buflen)
{
    uint8_t neg = 0;
    int32_t v, deg, frac;

    if (buf == NULL || buflen < 12) return;

    if (scaled < 0) { neg = 1; v = -scaled; } else { v = scaled; }

    deg  = v / GPS_COORD_SCALE;                  /* 度 */
    frac = v % GPS_COORD_SCALE;                  /* 小数部分(1e-6 度) */

    /* 输出 ddd.ffffff（度.小数度），固定 6 位小数 */
    {
        /* 手工格式化，避免浮点 */
        char tmp[16];
        uint8_t i = 0, j;
        int32_t f = frac;

        /* 先输出小数 6 位 */
        tmp[6] = '\0';
        for (j = 0; j < 6; j++) { tmp[5 - j] = (char)('0' + f % 10); f /= 10; }

        /* 再输出整数度 */
        {
            char ib[12];
            uint8_t k = 0;
            if (deg == 0) ib[k++] = '0';
            while (deg > 0) { ib[k++] = (char)('0' + deg % 10); deg /= 10; }
            /* 反转 */
            while (k > 0) { buf[i++] = ib[--k]; }
        }

        buf[i++] = '.';
        for (j = 0; j < 6; j++) buf[i++] = tmp[j];
        buf[i] = '\0';
        (void)neg;   /* 符号由调用方根据 N/S/E/W 处理，这里只输出绝对值 */
    }
}
