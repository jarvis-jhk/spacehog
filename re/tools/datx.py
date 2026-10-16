"""Allegro 3.11 datafile extractor (handles password-encrypted LZSS packfiles).

usage: datx.py FILE.DAT OUTDIR [--password skidmarks]

Writes one file per object (PNG for bitmaps/RLE sprites/fonts, WAV for samples,
SMF .mid for MIDI, .pal/.bin otherwise) plus index.json describing the tree.
"""
import sys, os, json, struct, zlib

F_PACK_MAGIC = 0x736C6821
F_NOPACK_MAGIC = 0x736C682E
N, F, THRESHOLD = 4096, 18, 2


def ident(s):
    return struct.unpack('>I', s.encode())[0]


def name4(x):
    return struct.pack('>I', x & 0xffffffff).decode('latin-1')


def encrypt(x, pw):
    mask = 0
    for i, c in enumerate(pw):
        mask ^= c << ((i & 3) * 8)
    return x ^ mask


def lzss_unpack(src, pw, pwpos=0):
    """Allegro pack_read(). pw XORs each flag byte (cycling)."""
    out = bytearray()
    text = bytearray(N)
    r = N - F
    flags = 0
    p = 0
    n = len(src)
    while True:
        flags >>= 1
        if (flags & 256) == 0:
            if p >= n:
                break
            c = src[p]; p += 1
            if pw:
                c ^= pw[pwpos]; pwpos = (pwpos + 1) % len(pw)
            flags = c | 0xFF00
        if flags & 1:
            if p >= n:
                break
            c = src[p]; p += 1
            text[r] = c; r = (r + 1) & (N - 1)
            out.append(c)
        else:
            if p + 1 >= n:
                break
            i = src[p]; j = src[p + 1]; p += 2
            i |= (j & 0xF0) << 4
            j = (j & 0x0F) + THRESHOLD
            for k in range(j + 1):
                c = text[(i + k) & (N - 1)]
                text[r] = c; r = (r + 1) & (N - 1)
                out.append(c)
    return bytes(out)


class R:
    def __init__(self, b, pw):
        self.b, self.p, self.pw = b, 0, pw

    def mgetl(self):
        v = struct.unpack_from('>i', self.b, self.p)[0]; self.p += 4; return v

    def mgetw(self):
        v = struct.unpack_from('>h', self.b, self.p)[0]; self.p += 2; return v

    def read(self, n):
        v = self.b[self.p:self.p + n]; self.p += n; return v

    def chunk(self):
        fsize = self.mgetl()
        dsize = self.mgetl()
        raw = self.read(abs(fsize))
        if dsize < 0:
            # packed chunk: password restarts from the beginning for the chunk
            return lzss_unpack(raw, self.pw), -dsize, True
        return raw, dsize, False


def png_write(path, w, h, pix, pal=None, trans0=False):
    """Write 8-bit indexed PNG (pal: list of 256 (r,g,b) 0..255)."""
    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    if pal is None:
        pal = [(i, i, i) for i in range(256)]
    raw = b''.join(b'\0' + bytes(pix[y * w:(y + 1) * w]) for y in range(h))
    out = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0))
    out += chunk(b'PLTE', b''.join(bytes(c) for c in pal))
    if trans0:
        out += chunk(b'tRNS', b'\0')
    out += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(out)


def rle_decode(w, h, data):
    """8-bit Allegro RLE: per row, signed counts: >0 literal run, <0 skip, 0 = eol."""
    pix = bytearray(w * h)
    p = 0
    for y in range(h):
        x = 0
        while True:
            c = struct.unpack_from('b', data, p)[0]; p += 1
            if c == 0:
                break
            if c > 0:
                pix[y * w + x:y * w + x + c] = data[p:p + c]; p += c; x += c
            else:
                x += -c
    return pix


class Extractor:
    def __init__(self, outdir, pal):
        self.outdir, self.pal = outdir, pal

    def fname(self, path, name, ext):
        safe = ''.join(ch if ch.isalnum() or ch in '_-.' else '_' for ch in name)
        d = os.path.join(self.outdir, *path)
        os.makedirs(d, exist_ok=True)
        return os.path.join(d, safe + ext)

    def obj(self, path, typ, data, name, props):
        t = name4(typ)
        e = {'name': name, 'type': t, 'size': len(data)}
        if props:
            e['props'] = props
        rd = R(data, b'')
        if t == 'BMP ':
            bits = rd.mgetw(); w = rd.mgetw(); h = rd.mgetw()
            e.update(bits=bits, w=w, h=h)
            if bits == 8:
                png_write(self.fname(path, name, '.png'), w, h, rd.read(w * h), self.pal)
        elif t == 'RLE ':
            bits = rd.mgetw(); w = rd.mgetw(); h = rd.mgetw(); size = rd.mgetl()
            e.update(bits=bits, w=w, h=h)
            if bits == 8:
                png_write(self.fname(path, name, '.png'), w, h, rle_decode(w, h, rd.read(size)), self.pal, True)
        elif t == 'PAL ':
            pal = [tuple(rd.b[i * 4 + k] * 255 // 63 for k in range(3)) for i in range(256)]  # raw RGB structs (r,g,b,filler), 6-bit
            open(self.fname(path, name, '.pal'), 'wb').write(data)
            e['first_colors'] = pal[:4]
        elif t == 'SAMP':
            bits = rd.mgetw(); freq = rd.mgetw() & 0xffff; ln = rd.mgetl()
            st = bits < 0; bits = abs(bits)
            e.update(bits=bits, freq=freq, len=ln, stereo=st)
            pcm = rd.read(ln * (2 if st else 1) * (bits // 8))
            import wave
            with wave.open(self.fname(path, name, '.wav'), 'wb') as wv:
                wv.setnchannels(2 if st else 1); wv.setsampwidth(bits // 8); wv.setframerate(freq)
                if bits == 16:  # allegro 16-bit samples are unsigned
                    pcm = bytes(b for i in range(0, len(pcm), 2) for b in struct.pack('<h', struct.unpack_from('<H', pcm, i)[0] - 32768))
                wv.writeframes(pcm)
        elif t == 'MIDI':
            div = rd.mgetw()
            tracks = []
            for c in range(32):
                ln = rd.mgetl()
                if ln > 0:
                    tracks.append(rd.read(ln))
            e.update(divisions=div, tracks=len(tracks))
            smf = b'MThd' + struct.pack('>IHHH', 6, 1, len(tracks), div)
            for tr in tracks:
                smf += b'MTrk' + struct.pack('>I', len(tr)) + tr
            open(self.fname(path, name, '.mid'), 'wb').write(smf)
        elif t == 'FONT':
            height = rd.mgetw()
            e['height'] = height
            if height > 0:
                bits = rd.read(224 * height)
                cols = 16; rows = 14
                W, H = cols * 8, rows * height
                pix = bytearray(W * H)
                for ch in range(224):
                    for y in range(height):
                        b = bits[ch * height + y]
                        for x in range(8):
                            if b & (0x80 >> x):
                                pix[((ch // cols) * height + y) * W + (ch % cols) * 8 + x] = 255
                png_write(self.fname(path, name, '.png'), W, H, pix)
            else:
                glyphs = []
                while rd.p < len(rd.b) and len(glyphs) < 224:
                    w = rd.mgetw(); h = rd.mgetw()
                    glyphs.append((w, h, rd.read(w * h)))
                e['glyphs'] = [(g[0], g[1]) for g in glyphs]
                H = max(g[1] for g in glyphs); cw = max(g[0] for g in glyphs) + 1
                cols = 16; rows = (len(glyphs) + cols - 1) // cols
                W = cols * cw
                pix = bytearray(W * rows * H)
                for n, (w, h, d) in enumerate(glyphs):
                    ox, oy = (n % cols) * cw, (n // cols) * H
                    for y in range(h):
                        pix[(oy + y) * W + ox:(oy + y) * W + ox + w] = d[y * w:(y + 1) * w]
                png_write(self.fname(path, name, '.png'), W, rows * H, pix, self.pal)
        elif t == 'FILE':
            e['children'] = self.file(path + [name], data)
        else:
            open(self.fname(path, name, '.' + t.strip().lower() + '.bin'), 'wb').write(data)
        return e

    def file(self, path, data):
        rd = R(data, self.pw)
        count = rd.mgetl()
        items = []
        props = {}
        while len(items) < count:
            typ = rd.mgetl()
            if name4(typ) == 'prop':
                pt = rd.mgetl(); ln = rd.mgetl()
                props[name4(pt)] = rd.read(ln).decode('latin-1')
                continue
            d, dsize, packed = rd.chunk()
            name = props.get('NAME', f'obj{len(items)}')
            e = self.obj(path, typ, d[:dsize], name, {k: v for k, v in props.items() if k != 'NAME'})
            e['index'] = len(items)
            if packed:
                e['packed'] = True
            items.append(e)
            props = {}
        return items


def load(path, pw):
    raw = open(path, 'rb').read()
    magic = struct.unpack('>I', raw[:4])[0]
    if magic == encrypt(F_PACK_MAGIC, pw):
        data = lzss_unpack(raw[4:], pw)
    elif magic == encrypt(F_NOPACK_MAGIC, pw):
        data = raw[4:]
    else:
        raise SystemExit('unknown magic %08x' % magic)
    assert data[:4] == b'ALL.', data[:4]
    return data[4:]


def find_pal(items):
    for e in items:
        if e['type'] == 'PAL ':
            return e
    return None


def main():
    args = sys.argv[1:]
    pw = b'skidmarks'
    if '--password' in args:
        i = args.index('--password'); pw = args[i + 1].encode(); del args[i:i + 2]
    pal = None
    if '--pal' in args:
        i = args.index('--pal'); pb = open(args[i + 1], 'rb').read(); del args[i:i + 2]
        pal = [tuple(pb[c * 4 + k] * 255 // 63 for k in range(3)) for c in range(256)]
    src, outdir = args
    data = load(src, pw)
    os.makedirs(outdir, exist_ok=True)
    ex = Extractor(outdir, pal)
    ex.pw = pw
    items = ex.file([], data)
    json.dump(items, open(os.path.join(outdir, 'index.json'), 'w'), indent=1)
    def show(items, ind=0):
        for e in items:
            extra = ' '.join(f'{k}={e[k]}' for k in ('w', 'h', 'bits', 'freq', 'len', 'height', 'divisions', 'tracks') if k in e)
            print(' ' * ind + f"{e['index']:3d} {e['type']} {e['name']:<28s} {e['size']:7d} {extra} {e.get('props', '')}")
            if 'children' in e:
                show(e['children'], ind + 2)
    show(items)


if __name__ == '__main__':
    main()
