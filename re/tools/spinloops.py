"""List loops in a C file that poll volatile input/timer state but never yield.
usage: spinloops.py FILE [--fix]   (--fix inserts wait_tick(); at the top of such loop bodies)"""
import re, sys
p = sys.argv[1]; fix = '--fix' in sys.argv
s = open(p).read()
YIELD = re.compile(r'\b(wait_tick|vsync|sys_idle|sys_poll|rest|readkey|fade_in|fade_out|fade_from|f0a994|f10df0|intro_wait|intro_type|frame_wait)\s*\(')
POLL = re.compile(r'\bg_tick\b|\bkey\[|\bjoy\[|\bmouse_b\b|\bmouse_[xy]\b')
def match_brace(s, i):
    d = 0
    for j in range(i, len(s)):
        if s[j] == '{': d += 1
        elif s[j] == '}':
            d -= 1
            if d == 0: return j
    return -1
def match_paren(s, i):
    d = 0
    for j in range(i, len(s)):
        if s[j] == '(': d += 1
        elif s[j] == ')':
            d -= 1
            if d == 0: return j
    return -1
inserts = []
for m in re.finditer(r'\b(while|for)\s*\(|\bdo\s*\{', s):
    if m.group(0).startswith('do'):
        b = s.index('{', m.start()); e = match_brace(s, b)
        tail = re.match(r'\s*while\s*\(', s[e + 1:])
        cond = ''
        if tail:
            ps = e + 1 + tail.end() - 1; pe = match_paren(s, ps); cond = s[ps:pe + 1]
        body = s[b:e + 1]
    else:
        ps = s.index('(', m.start()); pe = match_paren(s, ps); cond = s[ps:pe + 1]
        rest = s[pe + 1:]
        mm = re.match(r'\s*\{', rest)
        if not mm:
            # single statement or empty loop
            body = rest[:rest.index(';') + 1]; b = None
        else:
            b = pe + 1 + mm.end() - 1; e = match_brace(s, b); body = s[b:e + 1]
        if m.group(1) == "for" and not POLL.search(cond):
            continue
    if not POLL.search(cond + body) or YIELD.search(body + cond):
        continue
    line = s.count('\n', 0, m.start()) + 1
    print(f'{line}: {s[m.start():m.start()+60].splitlines()[0]}')
    if b is not None:
        inserts.append(b + 1)
if fix:
    for pos in sorted(inserts, reverse=True):
        s = s[:pos] + '\n    wait_tick();' + s[pos:]
    open(p, 'w').write(s)
    print('inserted', len(inserts))
