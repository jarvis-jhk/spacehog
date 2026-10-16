"""Remove function definitions (and their prototypes) from a C file by name.
usage: rmfunc.py FILE NAME..."""
import re, sys
p = sys.argv[1]; names = sys.argv[2:]
s = open(p).read()
for n in names:
    # definition: starts at a line beginning with a type and containing "n(" not ending with ';', ends at "\n}\n"
    m = re.search(r'(^/\* [^\n]*\*/\n)?\n?^[^\n;]*\b' + n + r'\s*\([^;{]*?\)\s*\n\{.*?^\}\n', s, re.M | re.S)
    if m:
        s = s[:m.start()] + s[m.end():]
    else:
        print('definition not found:', n)
    # only top-level prototypes (type at column 0), never call sites
    s = re.sub(r'^[A-Za-z_][\w \*]*\b' + n + r'\s*\([^;{()]*\);\n', '', s, flags=re.M)
open(p, 'w').write(s)
