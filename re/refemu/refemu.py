"""Differential tester: runs the original game code (orig/unpacked/SH.coff) in unicorn with
high-level emulation (HLE) of Allegro/libc, driven by a sync log recorded by the port
(`spacehog --sync-log FILE`), and compares the canonical game state at every sync point.

usage: refemu.py SYNCLOG [--data DIR] [--save-dir DIR] [--keep-going] [--trace-lib]
                 [--arg GAMEARG ...]
See re/refemu/README.md."""
import sys, os, struct, zlib, json, argparse, time, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE.rsplit('/', 2)[0]
sys.path.insert(0, HERE)
sys.path.insert(0, ROOT + '/re/tools')
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_BLOCK, \
    UC_HOOK_MEM_INVALID, UC_PROT_ALL
from unicorn.x86_const import *
import datx
import synclog

MEM_LO, MEM_HI = 0x1000, 0x2000000
HEAP0 = 0x100000
STACK_TOP = 0x1fe0000
EXIT_ADDR = 0x1fff000
TRAP_ADDR = 0x1fff100       # fake vtable entries point here
GDT_ADDR = 0x1ffe000
DATA_SEL = 0x10             # GDT index 2: flat data (also bitmap->seg for _putpixel/_getpixel)
VTABLE8 = 0x73cdc           # Allegro __linear_vtable8
MALLOC, FREE, REALLOC = 0x6af50, 0x6b094, 0x6b0c4
STUB_BANK = 0x47a24         # Allegro _stub_bank_switch: mov eax,[edx+eax*4+0x40]; ret

KEY = 0xcf9dc
SCREEN = 0x73de8
G_TICK, G_FPS, G_FRAMES = 0xa5f08, 0xcce08, 0xcce10
MOUSE_X, MOUSE_Y, MOUSE_B = 0x74750, 0x74754, 0x74758
JOY = {'left': 0xcfa74, 'right': 0xcfa78, 'up': 0xcfa84, 'down': 0xcfa88,
       'b0': 0xcfb58, 'b1': 0xcfb60, 'b2': 0xcfb68, 'b3': 0xcfb70}
PADBIT = {'left': 1, 'right': 2, 'up': 4, 'down': 8, 'b0': 16, 'b1': 32, 'b2': 64, 'b3': 128}
RAND_FPTR, RAND_RPTR, RAND_STATE = 0x8316c, 0x83170, 0x83174
SCRIBBLE = b'\x01' * 0x800
# Uninitialized locals the original reads ("still held" latches). Their value is whatever an
# earlier frame left on the stack; the port defines them as 1 ("held, release first"), so
# the emulated original gets the same value at function entry. {function: [entry_esp - X]}
UB_INIT = {
    0x12170: [0x234 - 4 * i for i in range(6, 134)],          # ship select local_234[6..133]
    0x257dc: [0xf0, 0xd0, 0x1a0],                             # key_config
    0x260cc: [0x128, 0xcc, 0xd8, 0xe0, 0xec],                 # gfx_options
    0x27308: [0x130, 0xd4, 0xe0, 0xe8, 0xf4],                 # options_menu
    0x1e300: [(0x108, 0), (0x208, 0)],                        # pilot_comments: both dead -> ""
}
SOUND_CALLS = {'set_volume', 'play_sample', 'adjust_sample', 'stop_sample', 'play_midi', 'stop_midi',
               'midi_pause', 'midi_resume'}
# game-level drawing/sound calls and their argument specs (must match src/game/calllog.h)
CALLS = {
    'clear_to_color': 'Ri', 'drawing_mode': 'iRii', 'solid_mode': '', 'set_clip': 'Riiii',
    'putpixel': 'Riii', 'line': 'Riiiii', 'rect': 'Riiiii', 'rectfill': 'Riiiii',
    'circle': 'Riiii', 'circlefill': 'Riiii', 'floodfill': 'Riii', 'blit': 'RRiiiiii',
    'draw_sprite': 'RRii', 'draw_lit_sprite': 'RRiii', 'draw_character': 'RRiii',
    'rotate_sprite': 'RRiii', 'stretch_sprite': 'RRiiii', 'draw_rle_sprite': 'RRii',
    'draw_lit_rle_sprite': 'RRiii', 'text_mode': 'i', 'textout': 'RRsiii',
    'textout_centre': 'RRsiii', 'set_volume': 'ii', 'play_sample': 'Riiii',
    'adjust_sample': 'Riiii', 'stop_sample': 'R', 'play_midi': 'Ri', 'stop_midi': '',
    'midi_pause': '', 'midi_resume': '',
}
DAT_FILES = {'spacehog.dat': 1, 'shlev.dat': 2, 'shbgs.dat': 3, 'shsnd.dat': 4}


def ident(s):
    return struct.unpack('>I', s.encode())[0]


class Stop(Exception):
    pass


class Emu:
    def __init__(self, opts):
        self.o = opts
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(MEM_LO, MEM_HI - MEM_LO, UC_PROT_ALL)
        d = open(ROOT + '/orig/unpacked/SH.coff', 'rb').read()
        _, _, tsize, dsize, bsize, entry, tstart, dstart = struct.unpack_from('<HHIIIIII', d, 20)
        uc.mem_write(tstart, d[0x100:0x100 + tsize])
        uc.mem_write(dstart, d[0x100 + tsize:0x100 + tsize + dsize])
        self.heap = HEAP0
        self.names = self.load_names()
        self.layout = json.load(open(HERE + '/layout.json'))
        self.recs = synclog.read(opts.synclog)
        self.nrec = 0
        self.keybuf = []
        self.pad = 0
        self.pal = bytes(1024)
        self.files = {}
        self.dat_objs = []          # (file id, obj index, start, size)
        self.dat_ranges = []
        self.mismatches = 0
        self.refs = {}
        self.ncreated = 0
        self.calls_crc = 0
        self.calls_text = []
        self.done = False
        self.last_sync_wall = time.time()
        self.where = ''
        self.setup_gdt()
        uc.mem_write(TRAP_ADDR, b'\xf4')
        uc.mem_write(EXIT_ADDR, b'\xf4')
        if opts.pixels:
            self.vtable = VTABLE8     # Allegro's own memory bitmap vtable: real drawing
        else:
            self.vtable = self.alloc(4 * 40)
            self.w32(self.vtable, 0)           # bitmap_type
            self.w32(self.vtable + 4, 8)       # color_depth
            self.w32(self.vtable + 8, 0)       # mask_color
            for i in range(3, 40):
                self.w32(self.vtable + 4 * i, TRAP_ADDR)
        self.passthru = {}
        self.blocks = {}
        self.hle = {}
        self.register_hle()
        self.sites = self.load_sites()
        for a in list(self.hle) + list(self.sites) + list(UB_INIT) + list(self.passthru):
            uc.hook_add(UC_HOOK_CODE, self.on_hook, begin=a, end=a)
        uc.hook_add(UC_HOOK_CODE, self.on_trap, begin=TRAP_ADDR, end=TRAP_ADDR)
        uc.hook_add(UC_HOOK_CODE, self.on_exit, begin=EXIT_ADDR, end=EXIT_ADDR)
        uc.hook_add(UC_HOOK_MEM_INVALID, self.on_badmem)
        if opts.trace_lib:
            self.lib_seen = set()
            uc.hook_add(UC_HOOK_BLOCK, self.on_libblock, begin=0x28050, end=0x73a00)

    # ------------------------------------------------------------ setup helpers
    def load_names(self):
        names = {}
        for l in open(ROOT + '/re/ghidra/out/functions.tsv'):
            a, n, nm = l.rstrip('\n').split('\t')
            names[int(a, 16)] = (nm, int(n))
        for l in open(ROOT + '/re/symbols.tsv'):
            if l.startswith('#') or not l.strip():
                continue
            f = l.rstrip('\n').split('\t')
            a = int(f[0], 16)
            names[a] = (f[1], names.get(a, ('', 0))[1])
        self.func_starts = sorted(names)
        return names

    def fname(self, a):
        import bisect
        i = bisect.bisect_right(self.func_starts, a) - 1
        if i < 0:
            return hex(a)
        s = self.func_starts[i]
        return f'{self.names[s][0]}+{a - s:#x}'

    def load_sites(self):
        sites = {}
        p = HERE + '/sites.tsv'
        if os.path.exists(p):
            for l in open(p):
                l = l.split('#')[0].strip()
                if not l:
                    continue
                f = l.split()
                sites[int(f[0], 16)] = f[1]
        return sites

    def setup_gdt(self):
        def desc(base, limit, access, flags):
            return struct.pack('<HHBBBB', limit & 0xffff, base & 0xffff, (base >> 16) & 0xff,
                               access, ((limit >> 16) & 0xf) | (flags << 4), (base >> 24) & 0xff)
        gdt = bytes(8) + desc(0, 0xfffff, 0x9a, 0xc) + desc(0, 0xfffff, 0x92, 0xc)
        self.uc.mem_write(GDT_ADDR, gdt)
        self.uc.reg_write(UC_X86_REG_GDTR, (0, GDT_ADDR, len(gdt) - 1, 0))
        # real descriptors everywhere: any segment load recomputes the SS 32-bit flag from
        # the SS cache, which unicorn leaves empty (pushes would silently become 16-bit)
        for r in (UC_X86_REG_SS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_FS, UC_X86_REG_GS):
            self.uc.reg_write(r, DATA_SEL)

    # ------------------------------------------------------------ memory helpers
    def r32(self, a):
        return struct.unpack('<I', self.uc.mem_read(a, 4))[0]

    def r32s(self, a):
        return struct.unpack('<i', self.uc.mem_read(a, 4))[0]

    def w32(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def rd(self, a, n):
        return bytes(self.uc.mem_read(a, n))

    def wr(self, a, b):
        self.uc.mem_write(a, bytes(b))

    def cstr(self, a, maxlen=4096):
        out = bytearray()
        while len(out) < maxlen:
            chunk = self.uc.mem_read(a + len(out), 64)
            i = chunk.find(0)
            if i >= 0:
                out += chunk[:i]
                break
            out += chunk
        return out.decode('latin-1')

    def alloc(self, n, zero=True):
        a = (self.heap + 15) & ~15
        self.heap = a + max(n, 1)
        assert self.heap < STACK_TOP - 0x100000, 'heap exhausted'
        return a

    def arg(self, i):
        return self.r32(self.uc.reg_read(UC_X86_REG_ESP) + 4 + 4 * i)

    def sarg(self, i):
        v = self.arg(i)
        return v - (1 << 32) if v & 0x80000000 else v

    # ------------------------------------------------------------ Allegro objects
    def make_bitmap(self, w, h, pixels=None, parent=None, x=0, y=0):
        b = self.alloc(0x40 + 4 * max(h, 1))
        if parent is None:
            pix = self.alloc(w * h)
            if pixels:
                self.wr(pix, pixels)
            lines = [pix + i * w for i in range(h)]
        else:
            pw = self.r32(parent)
            lines = [self.r32(parent + 0x40 + 4 * (y + i)) + x for i in range(h)]
            pix = lines[0] if lines else 0
        hdr = struct.pack('<16I', w, h, 0xffffffff, 0, w, 0, h, self.vtable, STUB_BANK, STUB_BANK,
                          pix, 0, 0, x, y, DATA_SEL)
        self.wr(b, hdr + struct.pack(f'<{h}I', *lines))
        return b

    def make_object(self, fid, typ, data):
        t = datx.name4(typ)
        rd = datx.R(data, b'')
        if t == 'BMP ':
            bits = rd.mgetw(); w = rd.mgetw(); h = rd.mgetw()
            return self.make_bitmap(w, h, rd.read(w * h))
        if t == 'RLE ':
            bits = rd.mgetw(); w = rd.mgetw(); h = rd.mgetw(); size = rd.mgetl()
            p = self.alloc(16 + size)
            self.wr(p, struct.pack('<4i', w, h, 8, size) + rd.read(size))
            return p
        if t == 'FONT':
            height = rd.mgetw()
            f = self.alloc(8)
            if height > 0:
                g = self.alloc(224 * height)
                self.wr(g, rd.read(224 * height))
                self.wr(f, struct.pack('<iI', height, g))
                return f
            fp = self.alloc(4 * 225)
            glyphs = []
            while rd.p < len(rd.b) and len(glyphs) < 224:
                w = rd.mgetw(); h = rd.mgetw()
                glyphs.append(self.make_bitmap(w, h, rd.read(w * h)))
            h0 = self.r32(glyphs[0] + 4)
            while len(glyphs) < 224:
                glyphs.append(self.make_bitmap(8, h0))
            self.wr(fp, struct.pack('<225I', *glyphs, 0))
            self.wr(f, struct.pack('<iI', -1, fp))
            return f
        if t == 'SAMP':
            bits = rd.mgetw(); freq = rd.mgetw() & 0xffff; ln = rd.mgetl()
            st = bits < 0; bits = abs(bits)
            pcm = rd.read(ln * (2 if st else 1) * (bits // 8))
            p = self.alloc(36 + len(pcm))
            self.wr(p, struct.pack('<9I', bits, int(st), freq, 128, ln, 0, ln, 0, p + 36) + pcm)
            return p
        if t == 'MIDI':
            div = rd.mgetw()
            p = self.alloc(4 + 32 * 8)
            tr = []
            for c in range(32):
                ln = rd.mgetl()
                if ln > 0:
                    q = self.alloc(ln)
                    self.wr(q, rd.read(ln))
                    tr += [q, ln]
                else:
                    tr += [0, 0]
            self.wr(p, struct.pack('<i64I', div, *tr))
            return p
        if t == 'FILE':
            return self.make_datafile(fid, data, nested=True)
        p = self.alloc(len(data) + 1)
        self.wr(p, data + b'\0')
        return p

    def make_datafile(self, fid, data, nested=False):
        rd = datx.R(data, self.password)
        count = rd.mgetl()
        arr = self.alloc(16 * (count + 1))
        c = 0
        while c < count:
            typ = rd.mgetl()
            if datx.name4(typ) == 'prop':
                rd.mgetl(); ln = rd.mgetl(); rd.read(ln)
                continue
            d, dsize, packed = rd.chunk()
            d = d[:dsize]
            p = self.make_object(fid, typ, d)
            self.wr(arr + 16 * c, struct.pack('<IIiI', p, typ, dsize, 0))
            if not nested:
                self.dat_ranges.append((p, p + dsize, (fid << 16) | (c + 1)))
                self.refs.setdefault(p, f'#d{fid}.{c}')
            c += 1
        self.wr(arr + 16 * count, struct.pack('<IIiI', 0, 0xffffffff, 0, 0))
        return arr

    # ------------------------------------------------------------ sync / compare
    SNAP_LO, SNAP_HI = 0x73a00, 0xd0400       # .data + .bss: all game state lives here

    def canonical(self):
        snap = self.rd(self.SNAP_LO, self.SNAP_HI - self.SNAP_LO)
        lo = self.SNAP_LO
        if not hasattr(self, 'ptr_fields'):
            self.ptr_fields = [[(f[1], f[2]) for f in s['fields'] if f[3] == 'ptr']
                               for s in self.layout['sections']]
        out = []
        for s, ptrs in zip(self.layout['sections'], self.ptr_fields):
            a = s['addr'] - lo
            raw = snap[a:a + s['count'] * s['stride']]
            if not ptrs:
                out.append(raw)
                continue
            parts = []
            st = s['stride']
            for e in range(s['count']):
                el = raw[e * st:(e + 1) * st]
                pos = 0
                for fo, fs in ptrs:
                    parts.append(el[pos:fo])
                    parts.append(self.norm_ptr(struct.unpack_from('<I', el, fo)[0]))
                    pos = fo + 4
                parts.append(el[pos:])
            out.append(b''.join(parts))
        out.append(b''.join(snap[a - lo:a - lo + n] for _, a, n in self.layout['misc']))
        fp, rp, st = struct.unpack_from('<III', snap, RAND_FPTR - lo)
        if lo <= st < self.SNAP_HI - 124:
            state = snap[st - lo:st - lo + 124]
        else:
            state = self.rd(st, 124)
        out.append(state + struct.pack('<II', (fp - st) // 4, (rp - st) // 4))
        return out

    def norm_ptr(self, p):
        if p == 0:
            return bytes(8)
        for a, b, oid in self.dat_ranges:
            if a <= p <= b:
                return struct.pack('<II', oid, p - a)
        return struct.pack('<II', 0xffffffff, 0)

    def section_names(self):
        return [s['name'] for s in self.layout['sections']] + ['misc', 'rng', 'calls', 'screen', 'pal']

    @staticmethod
    def calls_diff(mine, theirs, ctx=3):
        a, b = mine.decode('latin-1').splitlines(), theirs.decode('latin-1').splitlines()
        i = 0
        while i < len(a) and i < len(b) and a[i] == b[i]:
            i += 1
        print(f'  calls: {len(a)} in the original, {len(b)} in the port; first difference at #{i}:')
        for j in range(max(0, i - ctx), i):
            print(f'    = {a[j]}')
        for j in range(i, min(i + ctx, max(len(a), len(b)))):
            print(f'    orig: {a[j] if j < len(a) else "-"}')
            print(f'    port: {b[j] if j < len(b) else "-"}')

    def field_diff(self, si, mine, theirs, limit=40):
        names = self.section_names()
        lines = []
        if si < len(self.layout['sections']):
            s = self.layout['sections'][si]
            # canonical element size: stride + 4 per pointer
            esz = s['stride'] + 4 * sum(1 for f in s['fields'] if f[3] == 'ptr')
            for e in range(s['count']):
                a, b = mine[e * esz:(e + 1) * esz], theirs[e * esz:(e + 1) * esz]
                if a == b:
                    continue
                pos = 0
                for fname, fo, fs, kind in s['fields']:
                    n = 8 if kind == 'ptr' else fs
                    fa, fb = a[pos:pos + n], b[pos:pos + n]
                    if fa != fb:
                        lines.append(f'  {names[si]}[{e}].{fname}: orig={self.fmt(fa)} port={self.fmt(fb)}')
                    pos += n
                if len(lines) >= limit:
                    break
        elif names[si] == 'misc':
            pos = 0
            for lv, a, n in self.layout['misc']:
                fa, fb = mine[pos:pos + n], theirs[pos:pos + n]
                if fa != fb:
                    lines.append(f'  {lv}: orig={self.fmt(fa)} port={self.fmt(fb)}')
                pos += n
        else:
            lines.append(f'  rng: orig idx {struct.unpack_from("<II", mine, 124)} port {struct.unpack_from("<II", theirs, 124)}')
        return lines[:limit]

    @staticmethod
    def fmt(b):
        if len(b) == 4:
            return str(struct.unpack('<i', b)[0])
        if len(b) == 1:
            return str(b[0])
        if len(b) == 8:
            return '(%#x,+%d)' % struct.unpack('<II', b)
        return b.hex() if len(b) <= 32 else b[:32].hex() + '...'

    def sync(self, kind):
        try:
            rec = next(self.recs)
        except StopIteration:
            print(f'[refemu] port log ended after {self.nrec} records: no divergence'
                  if not self.mismatches else f'[refemu] log ended, {self.mismatches} mismatching records')
            self.done = True
            raise Stop()
        self.nrec += 1
        self.last_sync_wall = time.time()
        where = self.caller()
        if self.o.trace_from is not None and rec.idx >= self.o.trace_from:
            print(f'  sync {rec.idx} {kind} at {where} -> tick {rec.tick} pad {rec.pad} keys '
                  + ','.join(str(i) for i, v in enumerate(rec.key) if v))
        if rec.kind != kind:
            print(f'[refemu] record {rec.idx}: port did {rec.kind!r}, original does {kind!r} at {where}')
            self.done = True
            raise Stop()
        can = self.canonical()
        mine = [zlib.crc32(c) & 0xffffffff for c in can] + [self.calls_crc & 0xffffffff]
        scr = self.screen_bytes() if self.o.pixels else None
        mine.append((zlib.crc32(scr) & 0xffffffff) if scr else (0 if self.o.pixels else rec.hashes[len(mine)]))
        mine.append(zlib.crc32(self.pal_rgb()) & 0xffffffff)
        calls_text = b''.join(self.calls_text)
        self.calls_crc = 0
        self.calls_text = []
        bad = [i for i, c in enumerate(mine) if i < len(rec.hashes) and c != rec.hashes[i]]
        if bad:
            self.mismatches += 1
            names = self.section_names()
            if self.mismatches <= self.o.report:
                print(f'[refemu] record {rec.idx} ({kind} at {where}): state differs in '
                      + ', '.join(names[i] for i in bad))
                if rec.dump is not None:
                    pos = 0
                    for i, c in enumerate(can):
                        theirs = rec.dump[pos:pos + len(c)]
                        pos += len(c)
                        if i in bad:
                            print('\n'.join(self.field_diff(i, c, theirs)))
                    if len(can) in bad:
                        self.calls_diff(calls_text, rec.calls)
                    if (len(can) + 1 in bad or len(can) + 2 in bad) and rec.frame:
                        self.frame_diff(rec, scr)
                else:
                    print(f'  (rerun the port with --sync-dump {rec.idx} for field details)')
            if not self.o.keep_going:
                self.done = True
                raise Stop()
        self.apply_env(rec)
        return rec

    def apply_env(self, rec):
        self.wr(KEY, rec.key)
        self.keybuf = list(rec.keybuf)
        self.pad = rec.pad
        self.w32(MOUSE_X, rec.mouse[0]); self.w32(MOUSE_Y, rec.mouse[1]); self.w32(MOUSE_B, rec.mouse[2])
        self.w32(G_TICK, rec.tick); self.w32(G_FPS, rec.fps); self.w32(G_FRAMES, rec.frames)

    def caller(self):
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        eip = self.uc.reg_read(UC_X86_REG_EIP)
        if eip in self.hle:
            return f'{self.hle[eip][0]} from {self.fname(self.r32(esp) - 5)}'
        return self.fname(eip)

    # ------------------------------------------------------------ hooks
    def on_hook(self, uc, addr, size, _):
        try:
            if addr in UB_INIT:
                esp = uc.reg_read(UC_X86_REG_ESP)
                for off in UB_INIT[addr]:
                    off, val = off if isinstance(off, tuple) else (off, 1)
                    self.w32(esp - off, val)
                return
            if addr in self.passthru:          # log, then run the original code
                self.passthru[addr]()
                return
            if addr in self.sites:
                kind = self.sites[addr]
                if '?' in kind:         # 'p?15': only while key[15] is down
                    kind, k = kind.split('?')
                    if not self.rd(KEY + int(k), 1)[0]:
                        return
                self.sync(kind)
                return
            name, fn = self.hle[addr]
            # the real library code would leave its locals below the stack pointer; the
            # game reads such garbage in a few places (uninitialized locals), so scribble
            esp = uc.reg_read(UC_X86_REG_ESP)
            uc.mem_write(esp - 0x800, SCRIBBLE)
            ret = fn()
            if ret is not None:
                uc.reg_write(UC_X86_REG_EAX, ret & 0xffffffff)
            esp = uc.reg_read(UC_X86_REG_ESP)
            uc.reg_write(UC_X86_REG_EIP, self.r32(esp))
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
        except Stop:
            uc.emu_stop()
        except Exception:
            import traceback
            traceback.print_exc()
            print(f'[refemu] exception in HLE at {self.caller()}')
            self.done = True
            uc.emu_stop()

    def on_trap(self, uc, addr, size, _):
        esp = uc.reg_read(UC_X86_REG_ESP)
        print(f'[refemu] call into fake vtable from {self.fname(self.r32(esp))}')
        self.done = True
        uc.emu_stop()

    def on_exit(self, uc, addr, size, _):
        print(f'[refemu] main returned {uc.reg_read(UC_X86_REG_EAX)} after {self.nrec} records')
        self.done = True
        uc.emu_stop()

    def on_badmem(self, uc, access, addr, size, value, _):
        print(f'[refemu] bad memory access {access} at {addr:#x} from {self.fname(uc.reg_read(UC_X86_REG_EIP))}')
        self.done = True
        return False

    def on_libblock(self, uc, addr, size, _):
        if addr in self.names and addr not in self.lib_seen and addr not in self.hle:
            self.lib_seen.add(addr)
            esp = uc.reg_read(UC_X86_REG_ESP)
            print(f'[refemu] native lib call {self.names[addr][0]} from {self.fname(self.r32(esp))}')

    # ------------------------------------------------------------ pixels
    def screen_bytes(self):
        b = self.r32(SCREEN)
        if not b:
            return b''
        w, h = self.r32(b), self.r32(b + 4)
        return b''.join(self.rd(self.r32(b + 0x40 + 4 * y), w) for y in range(h))

    def pal_rgb(self):
        if getattr(self, '_pal_src', None) is not self.pal:
            rgb = bytearray(768)
            rgb[0::3] = self.pal[0::4]
            rgb[1::3] = self.pal[1::4]
            rgb[2::3] = self.pal[2::4]
            self._pal_src, self._pal_rgb = self.pal, bytes(rgb)
        return self._pal_rgb

    def frame_diff(self, rec, scr):
        from PIL import Image
        n = len(rec.frame) - 768
        port_pix, port_pal = rec.frame[:n], rec.frame[n:]
        out = os.path.dirname(os.path.abspath(self.o.synclog))
        h = n // 320
        def img(pix, pal):
            im = Image.frombytes('P', (320, len(pix) // 320), pix)
            im.putpalette(bytes(min(255, v * 255 // 63) for v in pal))
            return im.convert('RGB')
        a = img(scr, self.pal_rgb()) if scr else Image.new('RGB', (320, h))
        b = img(port_pix, port_pal)
        diff = Image.new('RGB', (320, h))
        nd = 0
        if scr:
            dp = diff.load()
            for i, (x1, x2) in enumerate(zip(scr, port_pix)):
                if x1 != x2:
                    dp[i % 320, i // 320] = (255, 0, 0)
                    nd += 1
        sheet = Image.new('RGB', (960, h))
        for k, im in enumerate((a, b, diff)):
            sheet.paste(im, (320 * k, 0))
        path = f'{out}/frame_{rec.idx}.png'
        sheet.resize((1920, h * 2), Image.NEAREST).save(path)
        print(f'  screen: {nd} pixels differ; palette {"differs" if self.pal_rgb() != port_pal else "equal"}; '
              f'{path} (original | port | diff)')

    # ------------------------------------------------------------ call log
    def created(self, p):
        self.refs[p] = f'#c{self.ncreated}'
        self.ncreated += 1
        return p

    def ref(self, p):
        if p == 0:
            return '#0'
        if p == self.r32(SCREEN):
            return '#scr'
        return self.refs.get(p, '#?')

    def pal_crc(self, p):
        raw = self.rd(p, 1024)
        rgb = b''.join(raw[i * 4:i * 4 + 3] for i in range(256))
        return 'pal:%08x' % (zlib.crc32(rgb) & 0xffffffff)

    def log_call(self, name, args):
        line = ' '.join([name] + args).encode('latin-1') + b'\n'
        self.calls_crc = zlib.crc32(line, self.calls_crc)
        self.calls_text.append(line)

    def make_logger(self, name, spec):
        def f():
            args, i = [], 0
            for c in spec:
                if c == 'R':
                    args.append(self.ref(self.arg(i)))
                elif c == 'i':
                    args.append(str(self.sarg(i)))
                elif c == 'P':
                    args.append(self.pal_crc(self.arg(i)))
                elif c == 's':
                    args.append('"' + self.cstr(self.arg(i)) + '"')
                i += 1
            self.log_call(name, args)
            return 0
        return f

    def log_textprintf(self, name):
        text = self.format(self.arg(5), 6)
        self.log_call(name, [self.ref(self.arg(0)), self.ref(self.arg(1)), '"' + text + '"',
                             str(self.sarg(2)), str(self.sarg(3)), str(self.sarg(4))])

    def format(self, fmtp, argi):
        import re
        fmt = self.cstr(fmtp)
        out = []
        pos = 0
        for m in re.finditer(r'%([-+ #0]*)(\d*)(?:\.(\d+))?(l|h)?([diuxXcs%])', fmt):
            out.append(fmt[pos:m.start()])
            pos = m.end()
            flags, width, prec, _, conv = m.groups()
            spec = '%' + flags + width + ('.' + prec if prec else '')
            if conv == '%':
                out.append('%')
                continue
            if conv == 's':
                v = self.cstr(self.arg(argi))
            elif conv in 'di':
                v = self.sarg(argi)
                conv = 'd'
            elif conv == 'c':
                v = self.arg(argi) & 0xff
            else:
                v = self.arg(argi)
            argi += 1
            out.append((spec + conv) % v)
        out.append(fmt[pos:])
        return ''.join(out)

    # ------------------------------------------------------------ HLE table
    def register_hle(self):
        h = self.hle
        A = self.arg
        nop = lambda: 0
        zero_fns = ['install_allegro_detect', 'allegro_exit', 'save_pcx', 'destroy_bitmap',
                    'unload_datafile', 'install_sound', 'destroy_midi', 'install_timer',
                    'install_keyboard', 'printf',
                    'fflush', 'gotoxy', 'textcolor', 'textbackground', 'clreol', 'clrscr',
                    'cprintf', '_go32_dpmi_lock_data', '_go32_dpmi_lock_code', 'install_int',
                    'delete_file']
        by_name = {v[0]: a for a, v in self.names.items()}
        for n in zero_fns:
            h[by_name[n]] = (n, nop)
        h[0x69ce0] = ('__main', nop)
        if not self.o.pixels:     # colour tables only matter for real drawing
            h[by_name['create_rgb_table']] = ('create_rgb_table', nop)
            h[by_name['create_light_table']] = ('create_light_table', nop)
        # game-level drawing/sound calls: logged like src/game/calllog.h. Without --pixels
        # they are no-ops; with --pixels drawing runs Allegro's own code after logging.
        native = self.o.pixels
        for n, spec in CALLS.items():
            if native and n not in SOUND_CALLS:
                self.passthru[by_name[n]] = self.make_logger(n, spec)
            else:
                h[by_name[n]] = (n, self.make_logger(n, spec))
        tbl = self.passthru if native else None
        def reg(name, fn):
            if native:
                self.passthru[by_name[name]] = fn
            else:
                h[by_name[name]] = (name, fn)
        reg('clear', lambda: self.log_call('clear_to_color', [self.ref(A(0)), '0']))
        reg('textprintf', lambda: self.log_textprintf('textout'))
        reg('textprintf_centre', lambda: self.log_textprintf('textout_centre'))
        if native:
            def malloc():
                n = A(0)
                p = self.alloc(n)
                self.sizes[p] = n
                return p
            def realloc():
                old, n = A(0), A(1)
                p = self.alloc(n)
                if old:
                    self.wr(p, self.rd(old, min(n, self.sizes.get(old, n))))
                self.sizes[p] = n
                return p
            self.sizes = {}
            h[MALLOC] = ('malloc', malloc)
            h[FREE] = ('free', lambda: 0)
            h[REALLOC] = ('realloc', realloc)
        h[by_name['install_mouse']] = ('install_mouse', lambda: 2)
        h[by_name['load_midi_x']] = ('load_midi', lambda: 0)
        h[by_name['bitmap_color_depth']] = ('bitmap_color_depth', lambda: 8)
        h[by_name['bitmap_mask_color']] = ('bitmap_mask_color', lambda: 0)

        def set_gfx_mode():
            if not self.r32(SCREEN):
                self.w32(SCREEN, self.make_bitmap(320, 200))
            return 0
        h[by_name['set_gfx_mode']] = ('set_gfx_mode', set_gfx_mode)
        h[by_name['create_bitmap']] = ('create_bitmap', lambda: self.created(self.make_bitmap(A(0), A(1))))
        h[by_name['create_sub_bitmap']] = ('create_sub_bitmap', lambda: self.created(
            self.make_bitmap(A(3), A(4), parent=A(0), x=A(1), y=A(2))))
        h[by_name['_make_bitmap']] = ('_make_bitmap', lambda: self.make_bitmap(A(0), A(1)))

        log_clip = self.make_logger('set_clip', CALLS['set_clip'])
        if self.o.pixels:
            self.passthru[by_name['set_clip']] = log_clip

        def set_clip():
            log_clip()
            b = A(0)
            x1, y1, x2, y2 = (self.sarg(i) for i in range(1, 5))
            if not (x1 or y1 or x2 or y2):
                self.wr(b + 8, struct.pack('<5i', 0, 0, self.r32(b), 0, self.r32(b + 4)))
            else:
                self.wr(b + 8, struct.pack('<5i', 1, max(x1, 0), min(x2 + 1, self.r32(b)), max(y1, 0),
                                           min(y2 + 1, self.r32(b + 4))))
        if not self.o.pixels:
            h[by_name['set_clip']] = ('set_clip', set_clip)

        def get_rle_sprite():
            b = A(0)
            p = self.alloc(16)
            self.wr(p, struct.pack('<4i', self.r32(b), self.r32(b + 4), 8, 0))
            return self.created(p)
        def get_rle_sprite_real():
            # Allegro 3.11 sprite.c, 8-bit case
            b = A(0)
            w, hh = self.r32(b), self.r32(b + 4)
            out = bytearray()
            for y in range(hh):
                row = self.rd(self.r32(b + 0x40 + 4 * y), w)
                run = -1
                for px in row:
                    if px:
                        v = out[run] if run >= 0 else 0
                        v = v - 256 if v > 127 else v
                        if run >= 0 and 0 < v < 127:
                            out[run] += 1
                        else:
                            run = len(out)
                            out.append(1)
                        out.append(px)
                    else:
                        v = out[run] if run >= 0 else 0
                        v = v - 256 if v > 127 else v
                        if run >= 0 and -128 < v < 0:
                            out[run] = (v - 1) & 0xff
                        else:
                            run = len(out)
                            out.append(0xff)
                out.append(0)
            p = self.alloc(16 + len(out))
            self.wr(p, struct.pack('<4i', w, hh, 8, len(out)) + bytes(out))
            return self.created(p)
        h[by_name['get_rle_sprite']] = ('get_rle_sprite',
                                        get_rle_sprite_real if self.o.pixels else get_rle_sprite)

        def set_pallete():
            self.log_call('set_pallete', [self.pal_crc(A(0))])
            self.sync('V')
            self.pal = self.rd(A(0), 1024)
        h[by_name['set_pallete']] = ('set_pallete', set_pallete)
        h[by_name['get_pallete']] = ('get_pallete', lambda: self.wr(A(0), self.pal))

        def fade_in():
            self.log_call('fade_in', [self.pal_crc(A(0)), str(self.sarg(1))])
            self.sync('F')
            self.pal = self.rd(A(0), 1024)
        h[by_name['fade_in']] = ('fade_in', fade_in)

        def fade_out():
            self.log_call('fade_out', [str(self.sarg(0))])
            self.sync('F')
            self.pal = bytes(1024)
        h[by_name['fade_out']] = ('fade_out', fade_out)

        def fade_from():
            self.log_call('fade_from', [self.pal_crc(A(0)), self.pal_crc(A(1)), str(self.sarg(2))])
            self.sync('F')
            self.pal = self.rd(A(1), 1024)
        h[by_name['fade_from']] = ('fade_from', fade_from)
        h[by_name['vsync']] = ('vsync', lambda: self.sync('V') and None)
        h[by_name['rest']] = ('rest', lambda: self.sync('R') and None)
        h[by_name['readkey']] = ('readkey', lambda: self.sync('K').ret)
        h[by_name['install_joystick']] = ('install_joystick', lambda: self.sync('J').ret)
        h[by_name['biostime']] = ('biostime', lambda: self.sync('B').ret)

        def clear_keybuf():
            self.keybuf = []
            self.wr(KEY, bytes(128))
        h[by_name['clear_keybuf']] = ('clear_keybuf', clear_keybuf)

        def poll_joystick():
            for k, a in JOY.items():
                self.w32(a, 1 if self.pad & PADBIT[k] else 0)
            return 0
        h[by_name['poll_joystick']] = ('poll_joystick', poll_joystick)

        def text_length():
            f, s = A(0), self.cstr(A(1)).encode('latin-1')
            if self.r32s(f) > 0:
                return len(s) * 8
            fp = self.r32(f + 4)
            n = 0
            for ch in s:
                c = ch - 32
                if c < 0 or c >= 224:
                    c = 0
                g = self.r32(fp + 4 * c)
                if g:
                    n += self.r32(g)
            return n
        h[by_name['text_length']] = ('text_length', text_length)

        def packfile_password():
            p = A(0)
            self.password = self.cstr(p).encode('latin-1') if p else b''
        self.password = b''
        h[by_name['packfile_password']] = ('packfile_password', packfile_password)

        def load_datafile():
            name = self.cstr(A(0))
            path = self.find_file(name, self.o.data)
            if not path:
                return 0
            data = datx.load(path, self.password)
            return self.make_datafile(DAT_FILES.get(os.path.basename(name).lower(), 9), data)
        h[by_name['load_datafile']] = ('load_datafile', load_datafile)

        # ---- libc files
        def fopen():
            name, mode = self.cstr(A(0)), self.cstr(A(1))
            if 'r' in mode:
                path = self.find_file(name, self.o.save_dir) or self.find_file(name, self.o.data)
                if not path:
                    return 0
            else:
                path = self.find_file(name, self.o.save_dir) or os.path.join(self.o.save_dir, name.lower())
            try:
                f = open(path, mode.replace('t', '') + ('b' if 'b' not in mode else ''))
            except OSError:
                return 0
            p = self.alloc(64)
            self.files[p] = f
            return p
        h[by_name['fopen']] = ('fopen', fopen)

        def fread():
            buf, sz, n, fp = A(0), A(1), A(2), A(3)
            d = self.files[fp].read(sz * n)
            self.wr(buf, d)
            return len(d) // sz if sz else 0
        h[by_name['fread']] = ('fread', fread)

        def fwrite():
            buf, sz, n, fp = A(0), A(1), A(2), A(3)
            self.files[fp].write(self.rd(buf, sz * n))
            return n
        h[by_name['fwrite']] = ('fwrite', fwrite)

        def fclose():
            f = self.files.pop(A(0), None)
            if f:
                f.close()
            return 0
        h[by_name['fclose']] = ('fclose', fclose)

        def exit_():
            print(f'[refemu] exit({self.sarg(0)}) after {self.nrec} records')
            self.done = True
            raise Stop()
        h[by_name['exit']] = ('exit', exit_)

    def find_file(self, name, d):
        name = name.replace('\\', '/').split('/')[-1]
        if not d or not os.path.isdir(d):
            return None
        for f in os.listdir(d):
            if f.lower() == name.lower():
                return os.path.join(d, f)
        return None

    # ------------------------------------------------------------ run
    def run(self, argv):
        uc = self.uc
        strs = []
        for a in argv:
            p = self.alloc(len(a) + 1)
            self.wr(p, a.encode() + b'\0')
            strs.append(p)
        av = self.alloc(4 * (len(strs) + 1))
        self.wr(av, struct.pack(f'<{len(strs) + 1}I', *strs, 0))
        envp = self.alloc(4)
        esp = STACK_TOP - 16
        self.wr(esp, struct.pack('<4I', EXIT_ADDR, len(strs), av, envp))
        uc.reg_write(UC_X86_REG_ESP, esp)
        uc.reg_write(UC_X86_REG_EBP, 0)
        pc = self.names_by('main', 0x27dbc)
        t0 = time.time()
        # One emu_start for the whole run: resuming after a stop would re-fire the hook at
        # the stop address (and consume a sync record twice). A watchdog thread stops a
        # run that no longer reaches sync points (a busy-wait site missing in sites.tsv).
        import threading
        stalled = []

        def watchdog():
            while not self.done:
                time.sleep(0.2)
                if time.time() - self.last_sync_wall > self.o.stall:
                    stalled.append(1)
                    uc.emu_stop()
                    return
        threading.Thread(target=watchdog, daemon=True).start()
        try:
            uc.emu_start(pc, EXIT_ADDR)
        except UcError as e:
            print(f'[refemu] CPU error {e} at {self.fname(uc.reg_read(UC_X86_REG_EIP))} after {self.nrec} records')
        if stalled and not self.done:
            self.report_stall()
        elif not self.done and uc.reg_read(UC_X86_REG_EIP) == EXIT_ADDR:
            left = sum(1 for _ in self.recs)
            print(f'[refemu] original main() returned after {self.nrec} records; port log has {left} more'
                  + (' (no divergence)' if not left and not self.mismatches else ''))
            if left:
                self.mismatches += 1
        self.done = True
        print(f'[refemu] {self.nrec} records in {time.time() - t0:.1f}s')
        return 0 if (self.mismatches == 0 and self.done) else 1

    def names_by(self, n, default):
        return default

    def report_stall(self):
        uc = self.uc
        hist = collections.Counter()
        for i in range(200):
            pc = uc.reg_read(UC_X86_REG_EIP)
            hist[pc] += 1
            uc.emu_start(pc, EXIT_ADDR, timeout=997)
        print(f'[refemu] stalled after record {self.nrec}: no sync for {self.o.stall}s. Hot spots:')
        for pc, n in hist.most_common(8):
            print(f'  {pc:#08x} {self.fname(pc)} x{n}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('synclog')
    ap.add_argument('--data', default=ROOT + '/orig/dist/game')
    ap.add_argument('--save-dir', default=ROOT + '/scratch/refemu_save')
    ap.add_argument('--keep-going', action='store_true')
    ap.add_argument('--report', type=int, default=5)
    ap.add_argument('--stall', type=float, default=10)
    ap.add_argument('--trace-lib', action='store_true')
    ap.add_argument('--pixels', action='store_true', help="run Allegro's drawing code, compare frames")
    ap.add_argument('--trace-from', type=int, help='print every sync record from this index on')
    ap.add_argument('--arg', dest='args', action='append', default=[], help='game argument (repeat)')
    o = ap.parse_args()
    os.makedirs(o.save_dir, exist_ok=True)
    e = Emu(o)
    sys.exit(e.run(['SH.EXE'] + o.args))


if __name__ == '__main__':
    main()
