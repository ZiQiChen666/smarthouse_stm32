# -*- coding: utf-8 -*-
"""
把 ui.c 中 #define 里的非 ASCII 字符串字面量转成 UTF-8 的 \\xNN 转义，
使 Keil AC5(按 GBK 解析源码) 也能正确编译，且字符串字节仍是 UTF-8。
只在 #define 行上操作，其它内容(含中文注释)原样保留。
"""
import re
import sys

PATH = "Core/Src/ui.c"

src = open(PATH, encoding="utf-8", newline="").read()

pattern = re.compile(r'(^[ \t]*#define[ \t]+\w+[ \t]+)"([^"\n]*)"([^\n]*)$', re.M)


def escape_literal(body):
    out = []
    for b in body.encode("utf-8"):
        if 32 <= b < 127 and b not in (0x22, 0x5C):   # 可见 ASCII, 排除 " 和 \
            out.append(chr(b))
        else:
            out.append("\\x%02X" % b)
    return "".join(out)


changed = 0


def repl(m):
    global changed
    body = m.group(2)
    if not any(ord(c) > 127 for c in body):
        return m.group(0)
    changed += 1
    rest = m.group(3)
    if "/*" not in rest:
        rest = "  /* %s */" % body
    return m.group(1) + '"' + escape_literal(body) + '"' + rest


new = pattern.sub(repl, src)

open(PATH, "w", encoding="utf-8", newline="").write(new)
print("converted #define literals:", changed)
