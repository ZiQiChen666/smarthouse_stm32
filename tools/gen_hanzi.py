# -*- coding: utf-8 -*-
"""
生成 OLED 中文界面的汉字表。
- 先列出"当前界面英文 -> 中文"的对照
- 再按类别给出预测会出现的汉字
- 最后去重，输出扁平字符表给取模软件(PCtoLCD2002 等)使用
"""
import os
import re
import sys
from collections import OrderedDict

# 以脚本所在位置推算工程根目录，保证在任意工作目录下都能运行
BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def P(*parts):
    return os.path.join(BASE, *parts)

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

CJK = re.compile(r'[\u4e00-\u9fff]')

# ---------- 1. 当前界面英文 -> 中文 ----------
MAPPING = [
    ("1.Measure",    "1.测量"),
    ("2.Curve",      "2.曲线"),
    ("3.Threshold",  "3.阈值"),
    ("4.Control",    "4.控制"),
    ("Temperature",  "温度"),
    ("Humidity",     "湿度"),
    ("Light",        "光照"),
    ("Waterlevel",   "水位"),
    ("T-MAX",        "温度上限"),
    ("T-MIN",        "温度下限"),
    ("H-MAX",        "湿度上限"),
    ("H-MIN",        "湿度下限"),
    ("L-MAX",        "光照上限"),
    ("L-MIN",        "光照下限"),
    ("W-MAX",        "水位上限"),
    ("W-MIN",        "水位下限"),
    ("FAN",          "风扇"),
    ("LIGHT",        "灯光"),
    ("PUMP",         "水泵"),
    ("TEMP :",       "温度"),
    ("HUMI :",       "湿度"),
    ("LIGHT:",       "光照"),
    ("WATER:",       "水位"),
    ("ON",           "开 / 已开"),
    ("OFF",          "关 / 已关"),
    ("> (光标)",     "选择 / 当前"),
    ("0-100",        "量程 0-100"),
    ("1/2  2/2",     "第1页 / 共2页"),
]

# ---------- 1b. 当前界面真正要用到的汉字（字体只做这些，越小越好） ----------
# 与 ui.c 里 UI_LANG_CN=1 的中文串保持一致；改动 ui.c 的中文串后，
# 在这里同步加一行字符串再重跑脚本即可。
NEEDED_STRINGS = [
    "测量界面", "曲线界面", "阈值界面", "控制界面",     # 主菜单
    "温度测量", "湿度测量", "光照测量", "水位测量",     # 测量界面
    "温度曲线", "湿度曲线", "光照曲线", "水位曲线",     # 曲线菜单
    "温度上限", "温度下限", "湿度上限", "湿度下限",     # 阈值界面
    "光照上限", "光照下限", "水位上限", "水位下限",
    "风扇控制", "灯光控制", "水泵控制",                 # 控制界面
    "开", "关",
]


def needed_chars(strings):
    out = OrderedDict()
    for s in strings:
        for ch in s:
            if CJK.match(ch) and ch not in out:
                out[ch] = 1
    return "".join(out.keys())


# ---------- 2. 预测汉字，按类别 ----------
CATS = OrderedDict()

CATS["一、界面/菜单/操作"] = (
    "主菜单 首页 主页 界面 页面 屏幕 菜单 曲线 阈值 控制 测量 项目 目录 层级 返回 退出 进入 切换 选择 确认 "
    "取消 保存 删除 修改 编辑 查询 设置 调整 添加 增加 减少 提升 降低 打开 关闭 启动 停止 "
    "暂停 继续 恢复 复位 重启 完成 等待 正在 请稍候 提示 帮助 说明 版本 关于 信息 上一步 "
    "下一步 上一页 下一页 第一页 最后一页 确定 应用 生效 输入 输出 显示 隐藏 刷新 清空 重置 "
    "全选 反选 全部 部分 单独 同时 依次 逐个 循环 轮换 交替 交换"
)

CATS["二、测量/传感器"] = (
    "测量 采集 采样 实时 数值 数据 读数 温度 湿度 光照 水位 距离 超声波 二氧化碳 浓度 "
    "加速度 陀螺仪 角度 姿态 倾斜 磁场 气压 大气压 海拔 经度 纬度 卫星 速度 航向 定位 卡片 "
    "射频 身份 电流 电压 功率 能量 电量 电池 充电 放电 传感器 探头 模块 芯片 信号 通道 "
    "光照度 勒克斯 温湿度 距离值 角度值 频率 周期 精度 误差 偏差 分辨率 灵敏度 量程 单位"
)

CATS["三、执行器/控制"] = (
    "风扇 灯光 照明 水泵 灌溉 继电器 电机 舵机 蜂鸣器 报警器 阀门 窗帘 插座 加热 制冷 "
    "除湿 加湿 通风 换气 灭菌 消毒 补水 喷淋 滴灌 洒水 空调 冰箱 电视 洗衣机 电灯 开关 "
    "执行 动作 开启 关闭 已开 已关 手动 自动 定时 联动 场景 模式"
)

CATS["四、阈值/参数"] = (
    "阈值 上限 下限 最大值 最小值 范围 门限 目标值 实际值 当前值 平均值 历史值 设定值 "
    "初始值 默认值 参数 系数 比例 增益 补偿 校准 标定 整定 起始 结束 初始化 配置 选项 "
    "功能 属性 状态 等级 优先级 采样点 记录 备份 存储 缓存 加载 导入 导出 读取 写入"
)

CATS["五、状态/提示"] = (
    "正常 异常 报警 故障 错误 成功 失败 完成 等待 连接 断开 在线 离线 强度 网络 服务器 "
    "云端 平台 上传 下载 发送 接收 同步 就绪 忙碌 空闲 运行 停机 保护 超时 无效 有效 "
    "合法 匹配 冲突 溢出 缺少 不足 充足 稳定 波动 变化 趋势 警告 提醒 注意 危险 安全 "
    "已开启 已关闭 工作中 待机 睡眠 唤醒 空闲 满载 过载 欠压 过压 过流 短路 断路 漏电"
)

CATS["六、单位/时间"] = (
    "摄氏度 度 百分比 米 厘米 千米 毫米 秒 分钟 小时 天 年 月 日 周 毫秒 微秒 升 毫升 "
    "克 千克 吨 立方米 平方米 倍 级 帕 焦耳 瓦 特 安 伏 欧姆 赫兹 分贝 转 圈 温度 湿度 "
    "压力 光照 时间 日期 星期 上午 下午 晚上 凌晨 中午 今天 明天 昨天 每天 每周 每月 每年"
)

CATS["七、数字/量词"] = (
    "零 一 二 三 四 五 六 七 八 九 十 百 千 万 亿 第一 第二 第三 第四 第 点 负 正 共 "
    "个 条 组 号 名称 编号 类型 值 单元 位 页 张 块 只 台 套 件 批 层 行 列 格 段 次 遍 "
    "半 双 对 若干 其他 其余 每个 各个 数量 次序 序号"
)

CATS["八、通用动词/形容词"] = (
    "是 有 无 不 可 以 请 按 键 操作 选择 显示 记录 设定 测试 检查 启用 禁用 允许 拒绝 "
    "支持 相关 需要 必须 能够 得到 使用 分配 占用 释放 计算 处理 分析 判断 比较 排序 "
    "过滤 搜索 定位 跟踪 记忆 复制 撤销 重做 转换 编码 解码 加密 解密 验证 授权 登录 "
    "注册 账户 密码 权限 管理员 用户 客人 访客 访问 请求 响应 提交 高 低 大 小 多 少 "
    "长 短 快 慢 好 坏 新 旧 冷 热 干 湿 亮 暗 满 空 强 弱 远 近 轻 重 正确 错误"
)

CATS["九、方向/环境"] = (
    "东 南 西 北 中 上 下 左 右 前 后 内 外 里 边 旁 侧 顶 底 端 头 尾 首 末 宽 窄 厚 薄 "
    "深 浅 环境 空气 质量 土壤 水分 甲醛 粉尘 烟雾 燃气 火焰 人体 红外 声音 噪声 温室 "
    "大棚 农业 养殖 种植 施肥 农药 病虫 害虫 收割 播种 生长 开花 结果 成熟 采摘 储藏 "
    "保鲜 冷藏 冷冻 干燥 潮湿 风 雨 雪 雷 电 晴 阴 云 日照 紫外 辐射 振动"
)

CATS["十、智能家居/场景"] = (
    "智能 家居 系统 设备 家庭 房间 客厅 卧室 厨房 卫生间 阳台 车库 门锁 安防 监控 门禁 "
    "感应 遥控 远程 本地 无线 蓝牙 语音 手势 触摸 亮度 色温 彩 暖 静音 震动 铃声 播放 "
    "音量 频道 节目 老人 儿童 宠物 花园 鱼缸 温度计 湿度计 布防 撤防 设防 防盗 防火 防水"
)

CATS["十一、定位/RFID/安全"] = (
    "卡片 卡号 身份 授权 通过 拒绝 有效 无效 刷卡 靠近 感应区 卫星 定位 经度 纬度 海拔 "
    "速度 方向 航向 卫星数 无定位 已定位 搜寻 入侵 报警 漏水 漏气 紧急 求助 呼叫 联系 "
    "电话 短信 通知 推送 消息 静音 安全 危险 密钥 认证 指纹 人脸 密码"
)

# ---------- 3. 去重 ----------
seen = OrderedDict()     # char -> first category
for cat, text in CATS.items():
    for ch in text:
        if CJK.match(ch) and ch not in seen:
            seen[ch] = cat

# 保证当前界面用到的字一定在表里
ui_chars = set()
for _, zh in MAPPING:
    for ch in zh:
        if CJK.match(ch):
            ui_chars.add(ch)
missing = sorted(ui_chars - set(seen))
for ch in missing:
    seen[ch] = "当前界面"

# ---------- 输出 ----------
lines = []
lines.append("# OLED 中文界面 - 汉字表 / 对照表\n")

lines.append("## 一、当前界面 英文 -> 中文 对照\n")
lines.append("| 英文(现状) | 中文建议 |")
lines.append("|---|---|")
for en, zh in MAPPING:
    lines.append("| `%s` | %s |" % (en, zh))

lines.append("\n## 二、预测汉字分类表\n")
for cat in CATS:
    chars = [c for c, cc in seen.items() if cc == cat]
    lines.append("**%s**（%d 字）\n" % (cat, len(chars)))
    lines.append("".join(chars) + "\n")

lines.append("## 三、扁平字符表（去重，共 %d 字）\n" % len(seen))
flat = "".join(seen.keys())
# 每行 40 字，方便查看
for i in range(0, len(flat), 40):
    lines.append(flat[i:i+40])

out_md = "\n".join(lines)

os.makedirs(P("docs"), exist_ok=True)
with open(P("docs", "hanzi.md"), "w", encoding="utf-8") as f:
    f.write(out_md)
with open(P("docs", "hanzi_list.txt"), "w", encoding="utf-8") as f:
    f.write(flat)

# ============================================================
# 4. 16x16 OLED 点阵取模  ->  Core/Inc/hz16.h + Core/Src/hz16.c
#    取模方式: 阴码(1=点亮) / 逐列式 / 顺向
#    每字 32 字节: 前16字节=上半页(第0~7行)，后16字节=下半页(第8~15行)
#                  每字节对应 1 列，bit0=该页最上行 (与工程内 F8X16 一致)
# ============================================================
FONT_PATH = r"C:/Windows/Fonts/simsun.ttc"
FONT_SIZE = 16
FONT_THRESH = 100          # 二值化阈值 (0~255)
HZ16_C = P("Core", "Src", "hz16.c")
HZ16_H = P("Core", "Inc", "hz16.h")

HZ16_HEADER = '''#ifndef __HZ16_H
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
'''

HZ16_INDEX_FUNC = '''
/* 取字形下标: 解码一个 UTF-8 字符, 在 HZ16_CODE[] 中线性查找 */
int HZ16_Index(const char *utf8)
{
    unsigned int code = 0;
    unsigned char c, n, k;
    unsigned short i;

    if (utf8 == 0) return -1;

    c = (unsigned char)utf8[0];
    if (c < 0x80u)                { code = c;        n = 0; }
    else if ((c & 0xE0u) == 0xC0u){ code = c & 0x1Fu; n = 1; }
    else if ((c & 0xF0u) == 0xE0u){ code = c & 0x0Fu; n = 2; }
    else if ((c & 0xF8u) == 0xF0u){ code = c & 0x07u; n = 3; }
    else return -1;

    for (k = 0; k < n; k++) {
        c = (unsigned char)utf8[1 + k];
        if ((c & 0xC0u) != 0x80u) return -1;
        code = (code << 6) | (c & 0x3Fu);
    }

    for (i = 0; i < HZ16_COUNT; i++)
        if (HZ16_CODE[i] == (unsigned short)code) return (int)i;

    return -1;
}
'''


def _glyph16(font, ch):
    from PIL import Image, ImageDraw
    img = Image.new("L", (16, 16), 0)
    ImageDraw.Draw(img).text((0, 0), ch, fill=255, font=font)
    px = img.load()
    data = []
    for band in range(2):        # 0=上8行, 1=下8行
        for x in range(16):      # 每页 16 列
            b = 0
            for bit in range(8):
                if px[x, band * 8 + bit] > FONT_THRESH:
                    b |= (1 << bit)   # bit0 = 该页最上行
            data.append(b)
    return data


def gen_font(flat, out_c=HZ16_C, out_h=HZ16_H):
    try:
        from PIL import ImageFont
    except ImportError:
        print("[font] 跳过: 未安装 Pillow, 请先执行  pip install pillow")
        return 0
    if not os.path.exists(FONT_PATH):
        print("[font] skip: font not found:", FONT_PATH)
        return 0
    os.makedirs(os.path.dirname(out_c), exist_ok=True)
    os.makedirs(os.path.dirname(out_h), exist_ok=True)

    font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
    entries = []
    codes = []
    blank = []
    for ch in flat:
        data = _glyph16(font, ch)
        if not any(data):
            blank.append(ch)
        entries.append((ch, data))
        codes.append(ord(ch))

    with open(out_h, "w", encoding="utf-8") as f:
        f.write(HZ16_HEADER)

    L = []
    L.append('#include "hz16.h"')
    L.append("")
    L.append("/* %d 个汉字, 每字 32 字节  阴码/逐列式/顺向 */" % len(flat))
    L.append("const unsigned short HZ16_COUNT = %d;" % len(flat))
    L.append("")
    L.append("const unsigned short HZ16_CODE[%d] = {" % len(flat))
    for i in range(0, len(codes), 12):
        L.append("    " + ",".join("0x%04X" % c for c in codes[i:i + 12]) + ",")
    L.append("};")
    L.append("")
    L.append("const unsigned char HZ16[%d][32] = {" % len(flat))
    for ch, data in entries:
        L.append("    {  /* %s U+%04X */" % (ch, ord(ch)))
        for i in range(0, 32, 8):
            L.append("        " + ",".join("0x%02X" % v for v in data[i:i + 8]) + ",")
        L.append("    },")
    L.append("};")
    L.append(HZ16_INDEX_FUNC)

    with open(out_c, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")

    print("[font] %s + %s  (%d 字, 共 %d 字节)" %
          (os.path.relpath(out_h, BASE), os.path.relpath(out_c, BASE),
           len(flat), len(flat) * 32))
    if blank:
        print("[font] WARN 空白字形:", "".join(blank))
    return len(flat)


needed = needed_chars(NEEDED_STRINGS) or flat
gen_font(needed)

print("font hanzi   =", len(needed), needed)
print("unique hanzi =", len(seen))
print("ui hanzi missing before fix =", len(missing))
per = OrderedDict((c, 0) for c in CATS)
for c, cc in seen.items():
    if cc in per:
        per[cc] += 1
for k, v in per.items():
    print("  %-22s %d" % (k, v))
print("flat=", flat)
