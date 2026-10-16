"""Generate the canonical game-state layout shared by the port's sync log and the
reference emulator (re/refemu).

  gen_sync.py            -> writes src/game/sync_state.c and re/refemu/layout.json

Canonical state = for every section (global array / scalar group) the fields of each
element in DOS layout order, raw little-endian bytes; pointer fields are replaced by an
8-byte (datafile object id, offset) pair so both sides compare equal.
Struct fields come from re/ghidra/types.h (names must exist in src/game/structs.h)."""
import re, json, os

ROOT = os.path.dirname(os.path.abspath(__file__)).rsplit('/', 2)[0]

# (section, port symbol, type, count, DOS address)
ARRAYS = [
    ('player', 'g_player', 'PLAYER', 2, 0xae088),
    ('enemy', 'g_enemy', 'ENEMY', 50, 0xae6c8),
    ('shot', 'g_shot', 'SHOT', 200, 0xa5f0c),
    ('pickup', 'g_pickup', 'PICKUP', 10, 0xae340),
    ('expl', 'g_expl', 'EXPLOSION', 50, 0xc8954),
    ('ebullet', 'g_ebullet', 'EBULLET', 100, 0xcac34),
    ('particle', 'g_particle', 'PARTICLE', 100, 0xa5348),
    ('event', 'g_event', 'LEVELEVENT', 256, 0xaa088),
    ('wave', 'g_wave', 'WAVE', 10, 0xccc68),
    ('bgobj', 'g_bgobj', 'BGOBJ', 10, 0xcaaf4),
    ('star', 'g_star', 'STAR', 255, 0xa9438),
    ('shiptype', 'g_shiptype', 'SHIPTYPE', 3, 0xc886c),
    ('levelhdr', '(&g_levelhdr)', 'LEVELHDR', 1, 0xc88fc),
    ('enemy_tmpl', 'g_enemy_tmpl', 'ENEMY', 40, 0xc8fd0),
    ('ebullet_tmpl', 'g_ebullet_tmpl', 'EBULLET', 5, 0xc8598),
    ('hiscore', 'g_hiscore', 'HISCORE', 10, 0xc0408),
]

# scalar globals grouped into one section: (port lvalue, DOS address, size)
MISC = [
    ('g_tick', 0xa5f08, 4), ('g_fps', 0xcce08, 4), ('g_frames', 0xcce10, 4),
    ('g_keys', 0xc8f94, 56), ('bg_tiles', 0xae5c0, 256), ('g_anim', 0x73c58, 4),
    ('difficulty', 0x73acc, 4), ('two_players', 0xa9434, 1), ('g_level', 0xccdf8, 4),
    ('g_title_y', 0xccc64, 4), ('snd_on', 0xc053c, 1), ('music_on', 0xa942c, 1),
    ('digi_vol', 0xa5f00, 1), ('midi_vol', 0xaa080, 1), ('opts', 0xc0520, 4, 'partial'),
    ('player_mortal', 0x73c08, 1), ('opt_cheats', 0x73c07, 1),
    ('music_playing', 0x73c0c, 1), ('g_ae6c4', 0xae6c4, 4), ('custom_level', 0xc8950, 4),
    ('custom_music', 0xcce18, 4), ('joy_present', 0xae33c, 1, 'expr'),
    ('bg_tile', 0x73ae0, 1), ('min_x', 0x73c34, 4), ('min_y', 0x73c38, 4),
    ('max_x', 0x73c3c, 4), ('max_y', 0x73c40, 4), ('anim_phase', 0x73c4e, 1),
    ('anim_tick', 0x73c4f, 1), ('frame_parity', 0x73c50, 1), ('particle_color', 0x73c5c, 4),
    ('firework_colors', 0x73c60, 20), ('launch_tick', 0xa5344, 4), ('view_y', 0xa5f04, 4),
    ('mothership_x', 0xa9430, 4), ('mouse_used', 0xaa07c, 1), ('step_ticks', 0xaa084, 4),
    ('level_step', 0xae338, 4), ('bg_scroll', 0xae4f8, 4), ('gameover_delay', 0xae5bc, 4),
    ('boss_pip_hp', 0xae6c0, 4), ('launch_step', 0xc03b4, 4), ('bosses_alive', 0xc052c, 4),
    ('level_done', 0xc0530, 4), ('engine_sound', 0xc0534, 4), ('boss_enemy', 0xc0538, 4),
    ('pad_used', 0xc8540, 1), ('boss_pips', 0xc8868, 4), ('mothership_y', 0xca6f0, 4),
]

PRIM = {'int': (4, 4), 'unsigned char': (1, 1), 'char': (1, 1), 'signed char': (1, 1),
        'long': (4, 4), 'unsigned int': (4, 4), 'short': (2, 2)}


def parse_types():
    src = open(ROOT + '/re/ghidra/types.h').read()
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
    structs = {}
    for m in re.finditer(r'typedef struct (\w+)\s*\{(.*?)\}\s*(\w+);', src, re.S):
        name, body = m.group(3), m.group(2)
        fields = []
        for decl in body.split(';'):
            decl = ' '.join(decl.split())
            if not decl:
                continue
            m2 = re.match(r'(.*?)(\w+)\s*(?:\[(\w+)\])?$', decl)
            typ, rest = m2.group(1).strip(), decl[len(m2.group(1)):]
            # "int a; int b" already split; handle "unsigned char lvl[9]" etc.
            for part in rest.split(','):
                part = part.strip()
                m3 = re.match(r'(\**)(\w+)\s*(?:\[(\w+)\])?$', part)
                ptr = m3.group(1) or typ.endswith('*')
                t = typ.rstrip('*').strip()
                cnt = int(m3.group(3), 0) if m3.group(3) else 1
                fields.append((t, bool(ptr), m3.group(2), cnt))
        structs[name] = fields
    return structs


def layout(structs, name, cache={}):
    """-> (size, align, [(fieldname, offset, size, kind, count)]) in DOS (i386) layout"""
    if name in cache:
        return cache[name]
    off, align, out = 0, 1, []
    for t, ptr, fname, cnt in structs[name]:
        if ptr:
            sz, al, kind = 4, 4, 'ptr'
        elif t in PRIM:
            (sz, al), kind = PRIM[t], 'raw'
        else:
            sz, al, _ = layout(structs, t)
            kind = 'raw'
        off = (off + al - 1) // al * al
        out.append((fname, off, sz * cnt if kind == 'raw' else sz, kind, cnt))
        off += sz * cnt
        align = max(align, al)
    size = (off + align - 1) // align * align
    cache[name] = (size, align, out)
    return cache[name]


def main():
    structs = parse_types()
    used = sorted({a[2] for a in ARRAYS}, key=lambda s: list(structs).index(s))
    c = ['/* Generated by re/tools/gen_sync.py -- do not edit.',
         ' * Canonical (DOS-layout) game state for the differential tester, see re/refemu. */',
         '#include "sh.h"', '#include "gglobals.h"', '#include "synclog.h"', '']
    js = {'sections': []}
    for st in used:
        size, _, fields = layout(structs, st)
        c.append(f'static void cs_{st}(const {st} *s)')
        c.append('{')
        for fname, off, sz, kind, cnt in fields:
            if kind == 'ptr':
                assert cnt == 1
                c.append(f'   sync_w_ptr(s->{fname});')
            else:
                c.append(f'   _Static_assert(sizeof(s->{fname}) == {sz}, "{st}.{fname}");')
                c.append(f'   sync_w_raw(&s->{fname}, {sz});')
        c.append('}')
        c.append('')
    c.append('void sync_state_section(int i)')
    c.append('{')
    c.append('   int k;')
    c.append('   switch (i) {')
    for i, (sec, sym, st, cnt, addr) in enumerate(ARRAYS):
        size, _, fields = layout(structs, st)
        c.append(f'   case {i}: for (k = 0; k < {cnt}; k++) cs_{st}(&{sym}[k]); break;')
        js['sections'].append({'name': sec, 'addr': addr, 'count': cnt, 'stride': size,
                               'fields': [[f, o, s, k] for f, o, s, k, n in fields]})
    i = len(ARRAYS)
    c.append(f'   case {i}: {{')
    mf = []
    moff = 0
    for lv, addr, sz, *flags in MISC:
        if 'expr' in flags:
            t = {1: 'unsigned char', 4: 'int'}[sz]
            c.append(f'      {{ {t} v = ({t})({lv}); sync_w_raw(&v, {sz}); }}')
            mf.append([lv, addr, sz])
            continue
        op = '>=' if 'partial' in flags else '=='
        c.append(f'      _Static_assert(sizeof({lv}) {op} {sz}, "{lv}");')
        c.append(f'      sync_w_raw((const void *)&{lv}, {sz});')
        mf.append([lv, addr, sz])
    c.append('      break;')
    c.append('   }')
    js['misc'] = mf
    c.append(f'   case {i + 1}: sync_w_rng(); break;')
    c.append('   }')
    c.append('}')
    c.append('')
    c.append(f'const int sync_num_sections = {i + 2};')
    c.append('const char *const sync_section_names[] = {')
    c.append('   ' + ', '.join(f'"{a[0]}"' for a in ARRAYS) + ', "misc", "rng"')
    c.append('};')
    open(ROOT + '/src/game/sync_state.c', 'w').write('\n'.join(c) + '\n')
    os.makedirs(ROOT + '/re/refemu', exist_ok=True)
    json.dump(js, open(ROOT + '/re/refemu/layout.json', 'w'), indent=1)
    print('sections', i + 2)


if __name__ == '__main__':
    main()
