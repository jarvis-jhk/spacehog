"""Mechanical first pass from Ghidra decompilation to compilable-ish C.

usage: d2c.py OUTDIR [ADDR...]   (default: all game functions)
Writes OUTDIR/fXXXXX.c per function and OUTDIR/globals_used.txt.
The output still needs a manual review: it is a starting point, not the port.
"""
import re, sys, os, struct, collections
sys.path.insert(0, os.path.dirname(__file__))
import peek

root = __file__.rsplit('/', 3)[0]
txt = open(root + '/re/ghidra/out/decomp.c').read()
parts = re.split(r'\n/\* ==== (\S+) @ ([0-9a-f]+) ==== \*/\n', txt)
funcs = {}
for i in range(1, len(parts), 3):
    funcs[int(parts[i + 1], 16)] = (parts[i], parts[i + 2])

sizes = {}
for l in open(root + '/re/ghidra/out/data.tsv'):
    if '::' in l: continue
    a, ln, ty, refs = l.rstrip('\n').split('\t')
    sizes[int(a, 16)] = (int(ln), ty)

TEXT_END = 0x73a00
TYPES = [
    (r'\bundefined4\b', 'int'), (r'\bundefined2\b', 'short'), (r'\bundefined1\b', 'char'),
    (r'\bundefined8\b', 'long long'), (r'\bundefined\b', 'char'), (r'\buint\b', 'unsigned int'),
    (r'\bushort\b', 'unsigned short'), (r'\buchar\b', 'unsigned char'), (r'\bbyte\b', 'unsigned char'),
    (r'\bulonglong\b', 'unsigned long long'), (r'\blonglong\b', 'long long'), (r'\bbool\b', 'int'),
    (r'\bcode\b', 'void'),
]

def c_string(addr):
    o = peek.off(addr)
    if o is None:
        return None
    end = peek.d.index(b'\0', o)
    s = peek.d[o:end]
    if len(s) > 200 or any(c < 9 or (13 < c < 32) for c in s):
        return None
    out = ''
    for c in s:
        ch = chr(c)
        if ch == '"': out += '\\"'
        elif ch == '\\': out += '\\\\'
        elif ch == '\n': out += '\\n'
        elif ch == '\t': out += '\\t'
        elif c >= 127: out += '\\x%02x""' % c
        else: out += ch
    return '"' + out + '"'

used = collections.Counter()

import json
DATN = {}
for f, pre, var in (('SPACEHOG', 'SH', 'spacehog'), ('SHLEV', 'LV', 'shlev'), ('SHBGS', 'BG', 'shbgs'), ('SHSND', 'SN', 'shsnd')):
    try:
        for e in json.load(open(f'{root}/orig/extracted/{f}/index.json')):
            DATN[(var, e['index'])] = pre + '_' + ''.join(c if c.isalnum() else '_' for c in e['name']).upper()
    except OSError:
        pass

def convert(name, body):
    s = body
    s = re.sub(r'/\* WARNING:[^*]*\*/\n?', '', s)
    # strings in .text referenced by address
    def strsub(m):
        a = int(m.group(1), 16)
        if a < TEXT_END:
            cs = c_string(a)
            if cs: return cs
        return m.group(0)
    s = re.sub(r'&DAT_000([0-9a-f]{5})\b', strsub, s)
    s = re.sub(r'\b_?DAT_000([0-9a-f]{5})\b', lambda m: (used.update([m.group(1)]), 'g' + m.group(1))[1], s)
    s = re.sub(r'\bPTR_DAT_000([0-9a-f]{5})\b', r'p\1', s)
    s = re.sub(r'\bPTR_FUN_000([0-9a-f]{5})\b', r'pf\1', s)
    s = re.sub(r'\b(?:thunk_)?FUN_000([0-9a-f]{5})\b', r'f\1', s)
    s = re.sub(r'\bLAB_000([0-9a-f]{5})\b', r'cb\1', s)
    s = re.sub(r'dat_(spacehog|shlev|shbgs|shsnd)\[(0x[0-9a-f]+|\d+)\]',
               lambda m: 'dat_%s[%s]' % (m.group(1), DATN.get((m.group(1), int(m.group(2), 0)), m.group(2))), s)
    for a, b in TYPES:
        s = re.sub(a, b, s)
    s = re.sub(r"'\\x01'", '1', s)
    s = re.sub(r"'\\0'", '0', s)
    return s

PORT = {}
def load_port_names():
    for l in open(root + '/re/port_names.tsv'):
        if l.startswith('#') or not l.strip(): continue
        a, b = l.rstrip('\n').split('\t')
        PORT[a] = b

def port_rename(s):
    def sub(m):
        return PORT.get(m.group(0), m.group(0))
    s = re.sub(r'\b[A-Za-z_][A-Za-z0-9_]*\b', sub, s)
    s = re.sub(r'\b_(g_\w+|bmp_\w+)\b', r'\1', s)
    s = s.replace('Ram000cce08', 'g_fps')
    # (&gXXXX)[i] where gXXXX became an array element -> fix common joy forms
    s = re.sub(r'\(&(joy\[0\]\.\w+(?:\[\d\])?)\)', r'(&\1)', s)
    return s

def main():
    port = '--port' in sys.argv
    if port:
        sys.argv.remove('--port')
        load_port_names()
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    sel = [int(a, 16) for a in sys.argv[2:]] or [a for a in funcs if 0x155c <= a < 0x28050]
    for a in sorted(sel):
        name, body = funcs[a]
        code = convert(name, body)
        if port:
            code = port_rename(code)
        open(f'{outdir}/f{a:05x}.c', 'w').write(f'/* {name} @ {a:#x} */\n' + code)
    with open(f'{outdir}/globals_used.txt', 'w') as f:
        for g, n in sorted(used.items()):
            a = int(g, 16)
            ln, ty = sizes.get(a, (0, '?'))
            f.write(f'{g}\t{n}\t{ln}\t{ty}\n')

main()
