"""Rename struct fields in the port and the Ghidra types in one go.
usage: rename_fields.py [--access-only] STRUCT ARRAY[,ARRAY...] OLD=NEW [OLD=NEW ...]

- renames the field declarations inside `typedef struct STRUCT {...} STRUCT;` in
  src/game/structs.h and re/ghidra/types.h,
- rewrites accesses `ARRAY[<balanced expr>].OLD` in src/game/*.c (also nested indexes),
- regenerates src/game/sync_state.c (re/tools/gen_sync.py).
Other access paths (pointers) are left for the compiler to point out."""
import os, re, sys, subprocess

ROOT = os.path.dirname(os.path.abspath(__file__)).rsplit('/', 2)[0]


def rename_in_struct(path, struct, pairs):
    s = open(path).read()
    m = re.search(r'typedef struct %s\b.*?\}\s*%s;' % (struct, struct), s, re.S)
    if not m:
        print('struct not found in', path)
        return
    body = m.group(0)
    for old, new in pairs:
        body, n = re.subn(r'\b%s\b' % re.escape(old), new, body)
        if n == 0:
            print(f'  {path}: {struct}.{old} not declared')
    s = s[:m.start()] + body + s[m.end():]
    open(path, 'w').write(s)


def rename_accesses(src, arrays, names):
    """-> (new text, count); handles accesses nested inside index expressions"""
    pat = re.compile(r'\b(%s)\[' % '|'.join(map(re.escape, arrays)))
    out, i, n = [], 0, 0
    while True:
        m = pat.search(src, i)
        if not m:
            out.append(src[i:])
            return ''.join(out), n
        j, depth = m.end(), 1
        while depth:
            depth += {'[': 1, ']': -1}.get(src[j], 0)
            j += 1
        inner, k = rename_accesses(src[m.end():j - 1], arrays, names)
        n += k
        out.append(src[i:m.end()] + inner + ']')
        i = j
        f = re.match(r'\.(\w+)\b', src[j:])
        if f and f.group(1) in names:
            out.append('.' + names[f.group(1)])
            i = j + f.end()
            n += 1


def main():
    args = sys.argv[1:]
    access_only = '--access-only' in args
    if access_only:
        args.remove('--access-only')
    struct, arrays = args[0], args[1].split(',')
    pairs = [a.split('=') for a in args[2:]]
    if not access_only:
        rename_in_struct(ROOT + '/src/game/structs.h', struct, pairs)
        rename_in_struct(ROOT + '/re/ghidra/types.h', struct, pairs)
    d = ROOT + '/src/game'
    total = 0
    for fn in sorted(os.listdir(d)):
        if not fn.endswith(('.c', '.h')) or fn in ('sync_state.c', 'structs.h'):
            continue
        p = os.path.join(d, fn)
        s = open(p).read()
        s, n = rename_accesses(s, arrays, dict(pairs))
        total += n
        open(p, 'w').write(s)
    print('accesses renamed:', total)
    subprocess.run([sys.executable, ROOT + '/re/tools/gen_sync.py'], check=True)


if __name__ == '__main__':
    main()
