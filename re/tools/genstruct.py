"""Generate a C struct for a global array from access patterns in the decompilation.
usage: genstruct.py NAME BASE STRIDE COUNT  -> prints struct (fields f_XX)"""
import sys, re, collections
root = __file__.rsplit('/', 3)[0]
name, base, stride, count = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 0), int(sys.argv[4], 0)
txt = open(root + '/re/ghidra/out/decomp.c').read()
sizes = collections.defaultdict(collections.Counter)
CAST = {'int': 4, 'uint': 4, 'undefined4': 4, 'float': 4, 'short': 2, 'ushort': 2, 'undefined2': 2,
        'char': 1, 'byte': 1, 'undefined1': 1, 'undefined': 1, 'BITMAP': 4, 'RLE_SPRITE': 4}
for m in re.finditer(r'(?:\*\((\w+) \*\*?\)\(?)?&?_?DAT_000([0-9a-f]{5})\)?(\s*\[[^\]]*\]|\s*\+\s*\(?\w+\s*\*\s*(?:0x[0-9a-f]+|\d+))?', txt):
    a = int(m.group(2), 16)
    if not (base <= a < base + stride * count):
        continue
    off = (a - base) % stride
    cast, idx = m.group(1), m.group(3) or ''
    ln = None
    if cast:
        ln = CAST.get(cast, 4)
    else:
        mm = re.search(r'\*\s*(0x[0-9a-f]+|\d+)', idx)
        if mm:
            mul = int(mm.group(1), 0)
            ln = {stride: 1, stride // 2: 2, stride // 4: 4}.get(mul)
    if ln and off + ln <= stride:
        sizes[off][ln] += 1
fields, pos = [], 0
for off in sorted(sizes):
    if off < pos: continue
    ln = max(sizes[off], key=lambda k: (sizes[off][k], k))
    if off % ln: ln = 1
    if off > pos:
        fields.append(f'   unsigned char pad_{pos:02x}[{off - pos}];')
    t = {1: 'unsigned char', 2: 'short', 4: 'int'}[ln]
    fields.append(f'   {t} f_{off:02x};')
    pos = off + ln
if pos < stride:
    fields.append(f'   unsigned char pad_{pos:02x}[{stride - pos}];')
print(f'typedef struct {name} {{ /* {stride:#x} bytes, {count} at {base:#x} */')
print('\n'.join(fields))
print(f'}} {name};')
