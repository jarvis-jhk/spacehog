"""Minimal ZMBV (DOSBox capture codec) decoder for 8-bit AVIs.
usage: zmbv.py FILE.avi OUTDIR [every_n]   -> writes frame_NNNNN.png (RGB)
Also importable: frames(path) yields (palette_rgb_bytes, pixels_bytes, w, h)."""
import struct, sys, os, zlib


def avi_chunks(data):
    """yield (fourcc, payload) of the movi list video chunks, in order"""
    i = data.find(b'movi')
    if i < 0:
        raise ValueError('no movi list')
    p = i + 4
    end = len(data)
    while p + 8 <= end:
        cc = data[p:p + 4]
        sz = struct.unpack_from('<I', data, p + 4)[0]
        if cc == b'LIST':
            p += 12
            continue
        if cc == b'idx1':
            break
        yield cc, data[p + 8:p + 8 + sz]
        p += 8 + sz + (sz & 1)


def avi_size(data):
    i = data.find(b'avih')
    w, h = struct.unpack_from('<II', data, i + 8 + 32)
    return w, h


def frames(path):
    data = open(path, 'rb').read()
    w, h = avi_size(data)
    pal = bytearray(768)
    bpp = 1
    cur = bytearray(w * h)
    z = None
    bw = bh = 16
    for cc, chunk in avi_chunks(data):
        if not cc.endswith(b'dc'):
            continue
        if len(chunk) == 0:
            yield bytes(pal), bytes(cur), w, h   # dropped / duplicate frame
            continue
        flags = chunk[0]
        if flags & 1:  # key frame
            hi, lo, comp, fmt, bw, bh = chunk[1:7]
            bpp = {4: 1, 5: 2, 6: 2, 7: 3, 8: 4}[fmt]
            z = zlib.decompressobj()
            raw = z.decompress(chunk[7:])
            if bpp == 1:
                pal[:] = raw[:768]
                cur = bytearray(raw[768:768 + w * h])
            else:
                cur = bytearray(raw[:w * h * bpp])
        else:
            raw = z.decompress(chunk[1:])
            p = 0
            if bpp == 1 and flags & 2:
                for k in range(768):
                    pal[k] ^= raw[k]
                p = 768
            nbx = (w + bw - 1) // bw
            nby = (h + bh - 1) // bh
            nb = nbx * nby
            info = raw[p:p + nb * 2]
            p += (nb * 2 + 3) & ~3
            old = bytes(cur)
            for b in range(nb):
                bx, by = (b % nbx) * bw, (b // nbx) * bh
                v0, v1 = info[b * 2], info[b * 2 + 1]
                xor = v0 & 1
                vx = (v0 if v0 < 128 else v0 - 256) >> 1
                vy = (v1 if v1 < 128 else v1 - 256) >> 1
                cw = min(bw, w - bx)
                chh = min(bh, h - by)
                if vx or vy:
                    for y in range(chh):
                        sy = by + y + vy
                        for x in range(cw):
                            sx = bx + x + vx
                            d = ((by + y) * w + bx + x) * bpp
                            if 0 <= sx < w and 0 <= sy < h:
                                s0 = (sy * w + sx) * bpp
                                cur[d:d + bpp] = old[s0:s0 + bpp]
                            else:
                                cur[d:d + bpp] = bytes(bpp)
                if xor:
                    n = cw * bpp
                    for y in range(chh):
                        row = ((by + y) * w + bx) * bpp
                        seg = raw[p:p + n]
                        cur[row:row + n] = bytes(a ^ b for a, b in zip(cur[row:row + n], seg))
                        p += n
        yield bytes(pal), bytes(cur), w, h


def to_image(pal, pix, w, h):
    from PIL import Image
    if len(pix) == w * h * 4:
        return Image.frombytes('RGBA', (w, h), pix, 'raw', 'BGRA').convert('RGB')
    im = Image.frombytes('P', (w, h), pix)
    im.putpalette(pal)
    return im.convert('RGB')


if __name__ == '__main__':
    path, out = sys.argv[1], sys.argv[2]
    every = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    os.makedirs(out, exist_ok=True)
    n = 0
    for k, (pal, pix, w, h) in enumerate(frames(path)):
        if k % every == 0:
            to_image(pal, pix, w, h).save(f'{out}/frame_{k:05d}.png')
            n += 1
    print('frames', k + 1, 'written', n)
