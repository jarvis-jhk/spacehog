"""Unpack a UPX 0.x djgpp2/coff executable by emulating its decompression stub.
Writes a plain DJGPP COFF (with stub) using the original section layout from the opt header."""
import sys, struct
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_INTR
from unicorn.x86_const import *
src, dst = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read()
blocks, last = struct.unpack_from('<H', d, 4)[0], struct.unpack_from('<H', d, 2)[0]
off = blocks * 512 - ((512 - last) if last else 0)
mg, ns, ts, sp, nsym, ohsz, fl = struct.unpack_from('<HHIIIHH', d, off)
_, _, tsize, dsize, bsize, entry, tstart, dstart = struct.unpack_from('<HHIIIIII', d, off + 20)
secs = [struct.unpack_from('<8sIIIIIIHHI', d, off + 20 + ohsz + 40 * i) for i in range(ns)]
mu = Uc(UC_ARCH_X86, UC_MODE_32)
MEM = 0x1000000
mu.mem_map(0, MEM)
for s in secs:
    name, pa, va, size, ptr = s[:5]
    if ptr and not (s[9] & 0x80):
        mu.mem_write(va, d[off + ptr: off + ptr + size])
mu.reg_write(UC_X86_REG_ESP, 0x200000)
mu.reg_write(UC_X86_REG_EDI, 0)
# original entry: DJGPP crt0 starts at text_start
target = tstart
state = {'n': 0}
def hook(uc, addr, size, ud):
    if addr == target:
        uc.emu_stop()
def intr(uc, no, ud):
    print('INT', hex(no), 'eax', hex(uc.reg_read(UC_X86_REG_EAX)), 'at', hex(uc.reg_read(UC_X86_REG_EIP)))
    uc.emu_stop()
mu.hook_add(UC_HOOK_CODE, hook, begin=target, end=target)
mu.hook_add(UC_HOOK_INTR, intr)
try:
    # skip "push ds; pop es" (selector loads confuse unicorn without a GDT)
    mu.emu_start(entry + 2, MEM, count=500_000_000)
except Exception as e:
    r = {n: hex(mu.reg_read(getattr(__import__('unicorn.x86_const', fromlist=['x']), 'UC_X86_REG_' + n))) for n in ('EIP', 'ESI', 'EDI', 'ECX', 'EDX', 'EAX')}
    sys.exit(f'emulation failed: {e} {r}')
eip = mu.reg_read(UC_X86_REG_EIP)
print('stopped at', hex(eip))
assert eip == target, 'did not reach original entry'
text = bytes(mu.mem_read(tstart, tsize))
data = bytes(mu.mem_read(dstart, dsize))
# rebuild COFF
stub = d[:off]
hdr = struct.pack('<HHIIIHH', 0x14c, 3, 0, 0, 0, 28, 0x10f)
textptr = 20 + 28 + 40 * 3
textptr = (textptr + 0xff) & ~0xff
dataptr = textptr + tsize
opt = struct.pack('<HHIIIIII', 0x10b, 0, tsize, dsize, bsize, tstart, tstart, dstart)
sh = (struct.pack('<8sIIIIIIHHI', b'.text', tstart, tstart, tsize, textptr, 0, 0, 0, 0, 0x20) +
      struct.pack('<8sIIIIIIHHI', b'.data', dstart, dstart, dsize, dataptr, 0, 0, 0, 0, 0x40) +
      struct.pack('<8sIIIIIIHHI', b'.bss', dstart + dsize, dstart + dsize, bsize, 0, 0, 0, 0, 0, 0x80))
body = hdr + opt + sh
body += b'\0' * (textptr - len(body)) + text + data
open(dst, 'wb').write(stub + body)
print('wrote', dst, len(stub + body), 'text', hex(tstart), hex(tsize), 'data', hex(dstart), hex(dsize), 'bss', hex(bsize))
