"""Disassemble a game function (or address range) from SH.coff with symbol names.
usage: dis.py ADDR [END]"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import peek
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
root = __file__.rsplit('/', 3)[0]
names = {}
for l in open(root + '/re/ghidra/out/functions.tsv'):
    a, n, nm = l.rstrip('\n').split('\t'); names[int(a, 16)] = (nm, int(n))
for l in open(root + '/re/symbols.tsv'):
    if l.startswith('#') or not l.strip(): continue
    f = l.rstrip('\n').split('\t'); names.setdefault(int(f[0], 16), (f[1], 0))
a = int(sys.argv[1], 16)
end = int(sys.argv[2], 16) if len(sys.argv) > 2 else a + max(names.get(a, ('', 64))[1], 16)
md = Cs(CS_ARCH_X86, CS_MODE_32)
o = peek.off(a)
import re
for i in md.disasm(peek.d[o:o + (end - a)], a):
    ops = re.sub(r'0x([0-9a-f]{4,6})\b', lambda m: (names[int(m.group(1), 16)][0] if int(m.group(1), 16) in names else m.group(0)), i.op_str)
    print(f'{i.address:06x}  {i.mnemonic:6s} {ops}')
