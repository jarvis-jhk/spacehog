"""Reader for the port's sync log (src/game/synclog.c).
usage: synclog.py FILE [--summary]   prints the record kinds run-length encoded"""
import struct, sys


class Rec:
    __slots__ = ('idx', 'kind', 'ret', 'hashes', 'key', 'keybuf', 'pad', 'numjoy',
                 'mouse', 'tick', 'fps', 'frames', 'dump', 'calls', 'frame', 'pos')


def read(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'SHSYNC1\n', 'not a sync log'
    p = 8
    idx = 0
    while p < len(d):
        r = Rec()
        r.pos = p
        kind, flags, nsec, _ = d[p:p + 4]
        r.idx = idx
        r.kind = chr(kind)
        r.ret = struct.unpack_from('<i', d, p + 4)[0]
        p += 8
        r.hashes = struct.unpack_from(f'<{nsec}I', d, p); p += 4 * nsec
        r.key = d[p:p + 128]; p += 128
        nkb = d[p]; p += 1
        r.keybuf = list(struct.unpack_from(f'<{nkb}i', d, p)); p += 4 * nkb
        (r.pad, r.numjoy, mx, my, mb, r.tick, r.fps, r.frames) = struct.unpack_from('<8i', d, p)
        r.mouse = (mx, my, mb)
        p += 32
        r.dump = None
        r.calls = None
        r.frame = None
        if flags & 1:
            n = struct.unpack_from('<I', d, p)[0]; p += 4
            r.dump = d[p:p + n]; p += n
            n = struct.unpack_from('<I', d, p)[0]; p += 4
            r.calls = d[p:p + n]; p += n
            n = struct.unpack_from('<I', d, p)[0]; p += 4
            r.frame = d[p:p + n]; p += n
        idx += 1
        yield r


if __name__ == '__main__':
    runs = []
    n = 0
    for r in read(sys.argv[1]):
        n += 1
        if runs and runs[-1][0] == r.kind:
            runs[-1][1] += 1
        else:
            runs.append([r.kind, 1, r.idx])
    print('records', n)
    print(' '.join(f'{k}{c if c > 1 else ""}' for k, c, _ in runs[:400]))
