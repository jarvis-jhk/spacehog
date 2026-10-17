"""Split a 2-player replay into the two machines' inputs for a netplay test.
usage: splitrec.py IN.rec HOST.rec GUEST.rec [--seed N]

Player 2's keys go to the guest, written as player 1's keys (the guest's player-1 keys
drive player 2 online); everything else goes to the host, except that some of the
global keys (Enter, Esc, pause, letters) are moved to the guest at random. 'q' (end)
goes to both, pads ('j', 'n') stay with the host."""
import argparse, random

P1 = [72, 80, 77, 75, 82, 0, 83]
P2 = [17, 31, 32, 30, 35, 0, 36]
GLOBAL = {28, 1, 123} | set(range(16, 26)) | set(range(30, 39)) | set(range(44, 51))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('inp'); ap.add_argument('host'); ap.add_argument('guest')
    ap.add_argument('--seed', type=int, default=1)
    o = ap.parse_args()
    r = random.Random(o.seed)
    host, guest = [], []
    owner = {}                         # scancode -> where its current press went
    for line in open(o.inp):
        if line.startswith('#') or line.startswith('seed') or not line.strip():
            continue
        f = line.split()
        if f[1] == 'q':
            host.append(line); guest.append(line); continue
        if f[1] == 'k':
            sc, down, asc = int(f[2]), f[3] == '1', int(f[4])
            if down:
                if sc in P2 and asc == 0:
                    owner[sc] = 'p2'
                elif sc in GLOBAL:
                    owner[sc] = 'guest' if r.random() < 0.3 else 'host'
                else:
                    owner[sc] = 'host'
            w = owner.get(sc, 'host')
            if w == 'p2':
                guest.append(f'{f[0]} k {P1[P2.index(sc)]} {f[3]} 0\n'); continue
            if w == 'guest':
                guest.append(line); continue
        host.append(line)
    for path, ev in ((o.host, host), (o.guest, guest)):
        with open(path, 'w') as out:
            out.write('# netplay test input (splitrec.py)\n')
            out.writelines(ev)


if __name__ == '__main__':
    main()
