"""Print the game-code call graph (callees per function) from the Ghidra export."""
import re, sys
root = __file__.rsplit('/', 3)[0]
txt = open(root + '/re/ghidra/out/decomp.c').read()
parts = re.split(r'\n/\* ==== (\S+) @ ([0-9a-f]+) ==== \*/\n', txt)
names = {}
for i in range(1, len(parts), 3):
    names[int(parts[i + 1], 16)] = parts[i]
GAME_END = 0x28050
for i in range(1, len(parts), 3):
    name, addr, body = parts[i], int(parts[i + 1], 16), parts[i + 2]
    if addr >= GAME_END: continue
    callees = []
    for m in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', body):
        n = m.group(1)
        for a, nm in names.items():
            pass
        if n in names.values() and n != name and n not in callees:
            callees.append(n)
    lines = body.count('\n')
    game = [c for c in callees if c.startswith('FUN_') or c in ('sh_setup',)]
    lib = [c for c in callees if c not in game]
    print(f'{addr:06x} {name:<14} {lines:5d}L  game: {" ".join(game)}')
    if '-l' in sys.argv: print(f'{"":30} lib: {" ".join(lib)}')
