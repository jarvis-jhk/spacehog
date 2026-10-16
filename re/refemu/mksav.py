"""Write a SPACEHOG.SAV (354 bytes, DOS layout, see src/game/setup.c) for tests.
usage: mksav.py OUT [--p1 0|1|2] [--p2 0|1|2] [--two 0|1] [--difficulty N] [--low-scores]
control: 0 keyboard, 1 gamepad, 2 mouse"""
import argparse, struct

NAMES = ['Free Lunch Design', 'Maximillian Drake', 'Sheila Longhart', 'Billy 68', 'Kraven Drax',
         'Shay Prumble', 'Brock Vixel', 'Maxim Friggot', 'Leloo Takenoo', 'Hantor Capole']
KEYS = [72, 80, 77, 75, 82, 83, 28, 17, 31, 32, 30, 35, 36, 37]

ap = argparse.ArgumentParser()
ap.add_argument('out')
ap.add_argument('--p1', type=int, default=0)
ap.add_argument('--p2', type=int, default=0)
ap.add_argument('--two', type=int, default=0)
ap.add_argument('--difficulty', type=int, default=3)
ap.add_argument('--low-scores', action='store_true', help='all high scores 0: every game enters a name')
o = ap.parse_args()
b = bytearray(354)
for i, n in enumerate(NAMES):
    b[i * 28:i * 28 + len(n)] = n.encode()
    struct.pack_into('<ii', b, i * 28 + 20, 0 if o.low_scores else 50000 - i * 5000, i % 3)
for i, k in enumerate(KEYS):
    struct.pack_into('<i', b, 280 + i * 4, k)
b[336:346] = bytes([1, 1, 1, 50, 0, 0, 0, 0, 0, 0])
b[346:354] = bytes([200, 200, 1, 1, o.difficulty, o.two, o.p1, o.p2])
open(o.out, 'wb').write(b)
