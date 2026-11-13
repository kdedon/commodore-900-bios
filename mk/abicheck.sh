#!/bin/sh
# mk/abicheck.sh -- check the ROM service table and write romabi.h.
#
#	sh mk/abicheck.sh [OUT]
#
# The service and variable lists are read from lkcrt.s and boot.c, checked
# against the linked ROM, and written to OUT (default build/romabi.h) for
# programs that call the ROM.  Run `make -f mk/host.mk rom' first.

set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
mwc=$root/mk/mwc1985
out=${1:-$root/build/romabi.h}
ROMABI=0x029e

cd "$root"
mkdir -p build/abi
sh "$mwc/ld" -i -R 0x00000000 -o build/abi/rom.out crt.o rom.o ramtest.o sio.o \
	hd.o wd.o boot.o diag.o trap.o conf.o ddt.o hires.o io.o loadseg.o \
	lceddt.o mbittest.o diagdisp.o porttest.o /lib/libc.a font.o >/dev/null
sh "$mwc/run" build/1985root/bin/nm build/abi/rom.out > build/abi/rom.nm 2>/dev/null || :

python3 -I - "$ROMABI" "$out" <<'EOF'
import re, sys
want, out = int(sys.argv[1], 16), sys.argv[2]
s = open('lkcrt.s').read()
blk = s[s.index('romabi_:'):]
blk = blk[:blk.index('\n/ ---')]
svc = re.findall(r'\tjp\t(\w+)_\n', blk)
nvar = int(re.search(r'\.word\t(\d+)\s+/ variables', blk).group(1))
c = open('boot.c').read()
body = c[c.index('char    *romvars[] = {'):]
body = body[body.index('{') + 1:body.index('};')]
var = [re.sub(r'\(char \*\)|&|\[.*?\]|/\*.*?\*/|\s', '', v) for v in body.split(',')]
var = [v for v in var if v]

syms = {}
for line in open('build/abi/rom.nm'):
    f = line.split()
    if len(f) == 3 and len(f[0]) == 8:
        syms[f[2]] = int(f[0], 16)
b = open('build/abi/rom.out', 'rb').read()
t = b[b[6] | b[7] << 8:]
err = []
if syms.get('romabi') != want:
    err.append('romabi is at %x, not %x' % (syms.get('romabi', 0), want))
if t[want:want + 4] != b'C9JT':
    err.append('no C9JT magic at %x' % want)
nsvc = t[want + 6] << 8 | t[want + 7]
if nsvc != len(svc):
    err.append('table holds %d services, %d listed' % (nsvc, len(svc)))
if nvar != len(var):
    err.append('%d variables declared, %d in romvars' % (nvar, len(var)))
for n, name in enumerate(svc):
    slot = t[want + 16 + 8 * n:want + 24 + 8 * n]
    if slot[:4] != b'\x5e\x08\x80\x00' or slot[6:] != b'\0\0':
        err.append('slot %d is not a long jp' % n)
    elif (slot[4] << 8 | slot[5]) != syms.get(name):
        err.append('slot %d does not reach %s' % (n, name))
for e in err:
    print('abicheck: ' + e)
if err:
    sys.exit(1)

h = ['/*', ' * ROM service table, generated from the ROM source.  Call service n',
     ' * at ROMABI + 16 + 8 * n; romvars (header offset 8) holds the',
     ' * addresses of ROM state, indexed by RV_*.  Both lists only grow.', ' */',
     '#define ROMABI\t\t0x%04xL' % want, '#define ROMABI_MAGIC\t"C9JT"',
     '#define ROMABI_VERSION\t1', '#define ROMABI_NSVC\t%d' % len(svc),
     '#define ROMABI_NVAR\t%d' % len(var), '',
     '#define ROMSVC(n)\t(ROMABI + 16 + 8 * (n))']
h += ['#define RS_%s\t%d' % (n.upper(), i) for i, n in enumerate(svc)]
h += ['']
names = {'dbgfmt': 'cursor', 'ddtvars.d_conlr': 'conlr'}
h += ['#define RV_%s\t%d' % (names.get(v, v.split('.')[-1]).upper(), i) for i, v in enumerate(var)]
open(out, 'w').write('\n'.join(h) + '\n')
print('abicheck: romabi at %04x, %d services, %d variables -> %s' % (want, len(svc), len(var), out))
EOF
