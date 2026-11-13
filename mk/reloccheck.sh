#!/bin/sh
# mk/reloccheck.sh -- prove the ROM still works when its code moves.
#
#	sh mk/reloccheck.sh
#
# Relinks the built objects with a pad after crt.o, so everything past the
# reset code shifts by PAD bytes, then:
#   1. flags any word that holds the address of a moved text symbol in the
#      normal link and still holds it in the padded one (a hard-coded address);
#   2. boots the padded ROM and drives the monitor and debugger.
# Run `make -f mk/host.mk rom' first.

set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
C900_ROOT=$root
. "$root/mk/emulator.sh"
emu_need "boot the relinked ROM"

PAD=${PAD:-256}
# Objects whose contents only look like text addresses.
ALLOW=${ALLOW:-dbgfmt hrfont}
mwc=$root/mk/mwc1985
work=$root/build/reloc
rm -rf "$work"
mkdir -p "$work"

OBJS=$(sed -n '/^OBJECTS=/,/^$/p' "$root/Makefile" | tr -d '\\' | sed 's/OBJECTS=crt.o//')
for o in crt.o $OBJS font.o bootfix prom; do
	[ -f "$root/$o" ] || { echo "reloccheck: $o missing; build the ROM first" >&2; exit 2; }
done

cd "$root"
n=0
: > "$work/pad.s"
while [ $n -lt $((PAD / 2)) ]; do echo "	.word	0" >> "$work/pad.s"; n=$((n + 1)); done
sh "$mwc/as" -go build/reloc/pad.o build/reloc/pad.s >/dev/null

link() {	# link OUT [PADOBJ]
	sh "$mwc/ld" -i -R 0x00000000 -o "$1" crt.o $2 $OBJS /lib/libc.a font.o >/dev/null
}
link build/reloc/a.out
link build/reloc/b.out build/reloc/pad.o
sh "$mwc/run" build/1985root/bin/nm build/reloc/a.out > build/reloc/a.nm 2>/dev/null || :

static=0
python3 -I - "$work" "$PAD" crt.o "$ALLOW" <<'EOF' || static=1
import sys
work, pad, crt = sys.argv[1], int(sys.argv[2]), sys.argv[3]
allow = sys.argv[4].split()
def lout(p):
    b = open(p, 'rb').read()
    gw = lambda o: b[o] | b[o+1] << 8
    sz = [(gw(8+4*i) << 16) | gw(10+4*i) for i in range(9)]
    t0 = gw(6)
    text = b[t0:t0+sz[0]]
    d0 = t0 + sz[0] + sz[1]
    return text, b[d0:d0+sz[3]+sz[4]]
at, ad = lout(work + '/a.out')
bt, bd = lout(work + '/b.out')
x = len(lout(crt)[0])
syms, dsyms, tsyms = {}, [], []
for line in open(work + '/a.nm'):
    f = line.split()
    if len(f) == 3 and len(f[0]) == 8:
        v = int(f[0][4:], 16)
        if f[0][:4] == '0000':
            tsyms.append((v, f[2]))
            if v >= x:
                syms.setdefault(v, f[2])
        elif f[1] in ('PD', 'SD'):
            dsyms.append((v, f[2]))
owner = lambda t, o: max([(v, n) for v, n in t if v <= o] or [(0, None)])[1]
# A text address is a segment-0 word (0x0000 or 0x8000) and an offset.
w = lambda s, o: s[o] << 8 | s[o+1]
ref = lambda s, o: o >= 2 and w(s, o - 2) in (0x0000, 0x8000) and w(s, o) in syms
bad = []
for o in range(x, len(at) - 1, 2):
    if ref(at, o) and w(bt, o + pad) == w(at, o) and owner(tsyms, o) not in allow:
        bad.append(('text', o, w(at, o)))
for o in range(0, len(ad) - 1, 2):
    if ref(ad, o) and w(bd, o) == w(ad, o) and owner(dsyms, o) not in allow:
        bad.append(('data', o, w(ad, o)))
for kind, o, v in bad:
    print('reloccheck: %s+%04x holds %04x (%s) unrelocated' % (kind, o, v, syms[v]))
sys.exit(1 if bad else 0)
EOF

(cd build/reloc && sh "$mwc/run" ../../bootfix b.out rom >/dev/null && sh "$mwc/run" ../../prom rom >/dev/null)

# A blank disk fails autoboot; "19" at the boot prompt enters the monitor.
fw=$work/fw
mkdir -p "$fw"
cp build/reloc/rom.H "$fw/bios_h.bin"
cp build/reloc/rom.L "$fw/bios_l.bin"
truncate -s 10M "$work/blank.img"
keys=$(printf '19?\nm\nd\n?\nh 10 4\nbd\n' | python3 -I -c '
import sys
print("".join("\\i" + ("\\n" if c == "\n" else c) for c in sys.stdin.read()), end="")')
"$C900_EMU" --firmware="$fw" --disk="$work/blank.img" --max=3000000000 \
	--input-mark='when ready' --input="$keys" --stop-on=none > "$work/console" 2>&1 || :

fail=0
for want in 'RAM top:' 'display this menu' '0014 000C'; do
	grep -qF "$want" "$work/console" || { echo "reloccheck: padded ROM never said: $want"; fail=1; }
done
[ $fail = 0 ] || { echo "--- console ---"; cat "$work/console"; }

[ $static = 0 ] && [ $fail = 0 ] || exit 1
echo "reloccheck: ROM relinked +$PAD bytes, no fixed addresses, monitor and debugger work"
