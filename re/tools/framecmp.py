"""Compare a DOSBox ZMBV capture with frames dumped by the port (SPACEHOG_DUMP_FRAMES).
usage: framecmp.py VIDEO.avi PORT_FRAME_DIR [--save-misses DIR]
Reports how many distinct original frames also occur in the port's output."""
import sys, os, glob, hashlib
sys.path.insert(0, os.path.dirname(__file__))
import zmbv
from PIL import Image
avi, pdir = sys.argv[1], sys.argv[2]
save = sys.argv[sys.argv.index('--save-misses') + 1] if '--save-misses' in sys.argv else None
port = {}
for f in glob.glob(pdir + '/*.ppm'):
    im = Image.open(f).convert('RGB')
    port[hashlib.md5(im.tobytes()).hexdigest()] = f
orig, last = [], None
for k, (pal, pix, w, h) in enumerate(zmbv.frames(avi)):
    if pix == last:
        continue
    last = pix
    im = zmbv.to_image(pal, pix, w, h)
    orig.append((k, hashlib.md5(im.tobytes()).hexdigest(), im))
miss = [(k, im) for k, hsh, im in orig if hsh not in port]
print(f'original distinct frames: {len(orig)}, found in port: {len(orig) - len(miss)}, missing: {len(miss)}')
if miss:
    print('first missing frame indexes:', [k for k, _ in miss[:20]])
    if save:
        os.makedirs(save, exist_ok=True)
        for k, im in miss[:50]:
            im.save(f'{save}/miss_{k:05d}.png')
