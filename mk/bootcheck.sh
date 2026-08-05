#!/bin/sh
# mk/bootcheck.sh -- boot the BUILT ROM in the emulator and require it to get
# as far as the reference ROM does.
#
#	sh mk/bootcheck.sh [rom.H rom.L]
#
# THIS IS THE GATE THAT OUTLIVES `make check'.  The md5 comparison against
# boot-{L,H}_V_1.0.bin can only ever pass for an unmodified V1.0: the moment
# this repository adds a disk layout or grows the ROM to 64K -- which are two
# of its three stated goals -- the reference dumps stop being the right answer
# and a build that matched them would mean the change had not been made.
#
# What survives the change is the QUESTION the md5 was standing in for: does
# this ROM still bring the machine up?  So the gate becomes behavioural.  The
# ROM runs its own diagnostics and prints them, in order, before handing over
# to the boot block; each line is a subsystem that answered.  Requiring all of
# them, in order, and then the hand-over, tests the ROM rather than a hash of
# it -- and it keeps working when the bytes deliberately change.
#
# It needs a hard-disk image because the C900 firmware autoboots: $C900_DISK,
# or the emulator's own disk/hdd.bin.

set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
C900_ROOT=$root
. "$root/mk/emulator.sh"

H=${1:-$root/rom.H}
L=${2:-$root/rom.L}
for f in "$H" "$L"; do
	[ -f "$f" ] || { echo "bootcheck: $f is missing; \`make -f mk/host.mk split' first" >&2; exit 2; }
done

emu_need "boot the ROM this build produced"
emuroot=$(cd "$(dirname "$(dirname "$C900_EMU")")" && pwd)
disk=${C900_DISK:-$emuroot/disk/hdd.bin}
[ -f "$disk" ] || { echo "bootcheck: no disk image at $disk; set C900_DISK" >&2; exit 2; }

# The emulator wants a firmware DIRECTORY with its own two names in it.
fw=$(mktemp -d "${TMPDIR:-/tmp}/bootcheck.XXXXXX")
trap 'rm -rf "$fw"' EXIT INT HUP TERM
cp "$H" "$fw/bios_h.bin"
cp "$L" "$fw/bios_l.bin"

# Run until the last line we are waiting for appears, then stop the machine.
# Not `--stop-on=idle': that channel wants scripted input to have been consumed
# first, and this gate feeds the machine nothing -- it only watches.  Polling
# the console and killing is the honest version of "we have seen enough", and
# it puts a wall-clock bound on a ROM that hangs instead of one on a ROM that
# is merely slow.
log=$fw/console
last='Automatic boot in progress'
"$C900_EMU" --firmware="$fw" --disk="$disk" --max="${C900_MAX:-4000000000}" \
	--stop-on=none > "$log" 2>&1 &
emupid=$!
waited=0
limit=${C900_BOOT_TIMEOUT:-300}
while [ $waited -lt $limit ]; do
	kill -0 $emupid 2>/dev/null || break
	grep -qF "$last" "$log" 2>/dev/null && break
	sleep 2
	waited=$((waited + 2))
done
kill $emupid 2>/dev/null || :
wait $emupid 2>/dev/null || :

# In order, and all of them.  A ROM that prints the banner and then stops
# before the MMU test has failed even though the first line matched.
fail=0
prev=0
for want in \
	'Commodore C900 diagnostics' \
	'Memory test' \
	'CIO1 (U78) register test: OK' \
	'CIO2 (U66) register test: OK' \
	'SCC  (U74) register test: OK' \
	'MMU  (U2) test: OK' \
	'Automatic boot in progress'
do
	n=$(grep -n -F -m1 "$want" "$log" | cut -d: -f1 || :)
	if [ -z "$n" ]; then
		echo "bootcheck: FAIL -- the ROM never said: $want"
		fail=1
	elif [ "$n" -lt "$prev" ]; then
		echo "bootcheck: FAIL -- out of order: $want"
		fail=1
	else
		prev=$n
	fi
done

if [ $fail != 0 ]; then
	echo "--- console ---"
	cat "$log"
	exit 1
fi
echo "bootcheck: the built ROM ran its diagnostics and handed over to the boot block"
