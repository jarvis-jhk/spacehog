"""Rewrite Ghidra 'field[expr + -K]' accesses (negative offsets from a padding array) into
proper fields. Handles nested brackets. usage: fixidx.py FILE"""
import sys, re
RULES = [  # (prefix, K, replacement_prefix, add)
    ('pad_39[', 9, 'wlvl[', 0), ('pad_39[', 8, 'wlvl[', 1),
    ('ship.maxlvl[', 9, 'ship.lvl[', 0), ('ship.maxlvl[', 8, 'ship.lvl[', 1),
]
p = sys.argv[1]; s = open(p).read()
def find_close(s, i):
    d = 0
    for j in range(i, len(s)):
        if s[j] == '[': d += 1
        elif s[j] == ']':
            d -= 1
            if d == 0: return j
    return -1
changed = 0
for pre, k, rep, add in RULES:
    i = 0
    while True:
        i = s.find(pre, i)
        if i < 0: break
        o = i + len(pre) - 1
        c = find_close(s, o)
        inner = s[o + 1:c]
        suf = f' + -{k}'
        if inner.endswith(suf):
            e = inner[:-len(suf)]
            new = rep + (e if add == 0 else f'{e} + {add}') + ']'
            s = s[:i] + new + s[c + 1:]
            changed += 1
        i += 1
# orbit angle: pad_7e[X * 0xc + -2] -> orbit[X - 1].angle
i = 0
while True:
    i = s.find('pad_7e[', i)
    if i < 0: break
    o = i + 6; c = find_close(s, o); inner = s[o + 1:c]
    m = re.fullmatch(r'(.*) \* 0xc \+ -2', inner, re.S)
    if m:
        s = s[:i] + f'orbit[{m.group(1)} - 1].angle' + s[c + 1:]; changed += 1
    i += 1
open(p, 'w').write(s)
print('changed', changed)
