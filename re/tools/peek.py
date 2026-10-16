"""Read values from the unpacked SH.coff by virtual address.
usage: peek.py ADDR [count] [fmt]   fmt: I (u32, default), i, B, H, s (string)"""
import sys, struct
d = open(__file__.rsplit('/', 3)[0] + '/orig/unpacked/SH.coff', 'rb').read()
_, _, tsize, dsize, bsize, entry, tstart, dstart = struct.unpack_from('<HHIIIIII', d, 20)
TEXTPTR = 0x100
def off(v):
    if tstart <= v < tstart + tsize: return TEXTPTR + v - tstart
    if dstart <= v < dstart + dsize: return TEXTPTR + tsize + v - dstart
    return None
def peek(v, n=1, fmt='I'):
    o = off(v)
    if o is None: return ['bss'] * n
    if fmt == 's':
        return [d[o:d.index(b'\0', o)].decode('latin-1')]
    sz = struct.calcsize('<' + fmt)
    return [struct.unpack_from('<' + fmt, d, o + i * sz)[0] for i in range(n)]
if __name__ == '__main__':
    a = int(sys.argv[1], 16); n = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    fmt = sys.argv[3] if len(sys.argv) > 3 else 'I'
    print(' '.join(hex(x) if isinstance(x, int) else str(x) for x in peek(a, n, fmt)))
