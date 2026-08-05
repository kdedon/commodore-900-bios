#!/bin/sh
# mk/1985root.sh -- stage the filesystem the 1985 tools expect to be running on.
#
#	sh mk/1985root.sh [DIR]
#
# The ROM's Makefile names its inputs the way a program on the machine names
# them: -I/usr/sys/z8001/h, /lib/libc.a, /lib/crts0.o.  Those are guest paths,
# and on a host they mean something only because `c900 --exec' reroots the
# guest's absolute paths at $N2ROOT.  This script builds that root.
#
# Everything in it comes from vendor/mwc-1985, which is already laid out as a
# guest root -- bin/, lib/ and usr/ standing at the paths they stand at on the
# machine -- so staging is a copy and a check rather than a composition.  See
# vendor/mwc-1985/SOURCES.md.  Nothing is fetched and no other repository is
# consulted; the emulator is the only thing this repository does not carry.
#
# The forward-ported COHERENT headers are NOT a substitute for the 1985 ones --
# they are a later system, and compiling against them does not give the 1985
# objects.  Neither is COHERENT 0.8's tree.
#
# Setting C900_COH_IMAGE reads the library and the headers out of a stock
# COHERENT 0.7.3 hard-disk image instead, which is how the vendored copies are
# checked against a real disk.  The TOOLS still come from vendor/, and that
# path also needs the emulator, for its tools/disk.py.
#
# Variables: C900_1985ROOT (or the one argument) names where the root is built.

set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
C900_ROOT=$root
. "$root/mk/emulator.sh"

dest=${1:-${C900_1985ROOT:-$root/build/1985root}}
mwc=$root/vendor/mwc-1985

[ -f "$mwc/lib/cc0" ] && [ -f "$mwc/bin/as" ] || {
	echo "1985root.sh: no 1985 system at $mwc." >&2
	echo "  It is tracked in this repository -- the passes, the assembler," >&2
	echo "  the linkage editor, the C library and the header trees; see its" >&2
	echo "  SOURCES.md.  A checkout that does not have it is incomplete." >&2
	exit 2; }

rm -rf "$dest"
mkdir -p "$dest/lib" "$dest/bin" "$dest/tmp"

img=${C900_COH_IMAGE:-}
if [ -z "$img" ]; then
	# The vendored library, C runtime startoff and three header trees.
	cp "$mwc/lib/libc.a" "$mwc/lib/crts0.o" "$dest/lib/"
	cp -R "$mwc/usr" "$dest/usr"
	src=$mwc
else
	# A stock image instead: this is how the vendored copies are checked
	# against a real disk.  It needs the emulator, for its tools/disk.py.
	[ -f "$img" ] || { echo "1985root.sh: C900_COH_IMAGE=$img is not a file" >&2; exit 2; }
	emu_need "unpack a COHERENT disk image (its tools/disk.py reads the filesystem)"
	emuroot=$(cd "$(dirname "$(dirname "$C900_EMU")")" && pwd)
	disk=$emuroot/tools/disk.py
	[ -f "$disk" ] || { echo "1985root.sh: no $disk in the emulator checkout" >&2; exit 2; }
	mkdir -p "$dest/usr/sys/z8001" "$dest/usr/sys" "$dest/usr"
	py=${PYTHON:-python3}
	"$py" "$disk" "$img" --read --dest "$dest/lib" /lib/libc.a /lib/crts0.o >/dev/null
	"$py" "$disk" "$img" --read --dest "$dest/usr/sys/z8001" /usr/sys/z8001/h >/dev/null
	"$py" "$disk" "$img" --read --dest "$dest/usr/sys" /usr/sys/h >/dev/null
	"$py" "$disk" "$img" --read --dest "$dest/usr" /usr/include >/dev/null
	src=$img
fi

# nld compares libc.a's mtime against the ranlib stamp INSIDE the archive and
# says "outdated ranlib" when the file looks newer -- which a file just written
# by the extractor always does.  The archive is from June 1985; date it so.
touch -t 198506050000 "$dest/lib/libc.a" 2>/dev/null || :

for f in "$dest/lib/libc.a" "$dest/lib/crts0.o" \
         "$dest/usr/sys/z8001/h/canon.h" "$dest/usr/include/dir.h"; do
	[ -f "$f" ] || { echo "1985root.sh: $src does not have $(echo "$f" | sed "s|^$dest||")" >&2; exit 2; }
done

# The tools, from vendor/ in both cases.  They go in the root so the wrappers
# have one place to look, and so `nm' and `size' are to hand when an object has
# to be explained.  VARIANT travels with them: it is the word these passes must
# be run with, and the wrong word silently produces objects that are not the
# ones the machine shipped.
for f in cpp cc0 cc1 cc2; do cp "$mwc/lib/$f" "$dest/lib/$f"; done
for f in as nld nm size; do cp "$mwc/bin/$f" "$dest/bin/$f"; done
cp "$mwc/VARIANT" "$dest/VARIANT"
chmod 755 "$dest"/lib/cc? "$dest"/lib/cpp "$dest"/bin/*

echo "1985root.sh: staged $dest from $src"
