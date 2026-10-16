"""Print decompiled functions from re/ghidra/out/decomp.c by address or name.
usage: fn.py ADDR|NAME [...] [-n MAXLINES]"""
import sys, re
args = sys.argv[1:]
maxl = 10**9
if '-n' in args:
    i = args.index('-n'); maxl = int(args[i + 1]); del args[i:i + 2]
txt = open(__file__.rsplit('/', 3)[0] + '/re/ghidra/out/decomp.c').read()
parts = re.split(r'\n/\* ==== (\S+) @ ([0-9a-f]+) ==== \*/\n', txt)
idx = {}
for i in range(1, len(parts), 3):
    idx[parts[i]] = idx[parts[i + 1].lstrip('0')] = (parts[i], parts[i + 1], parts[i + 2])
for a in args:
    k = a.lower().removeprefix('0x').lstrip('0') if re.fullmatch(r'(0x)?[0-9a-fA-F]+', a) else a
    if k not in idx:
        print('not found', a); continue
    n, ad, body = idx[k]
    lines = body.strip('\n').split('\n')
    print(f'// ==== {n} @ {ad} ({len(lines)} lines)')
    print('\n'.join(lines[:maxl]))
