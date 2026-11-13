#!/bin/sh
# mk/serialcheck.sh -- boot over serial: send the ROM's own load module by
# XMODEM to (sx,1) in load-only mode and require the loader to accept it.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
C900_ROOT=$root
. "$root/mk/emulator.sh"
emu_need "boot over serial"

work=$root/build/serial
rm -rf "$work"
mkdir -p "$work/fw"
cp "$root/rom.H" "$work/fw/bios_h.bin"
cp "$root/rom.L" "$work/fw/bios_l.bin"
truncate -s 10M "$work/blank.img"
python3 -I "$here/xsend.py" "$work/sock" "$root/rom.out" > "$work/send.log" 2>&1 &
sender=$!
sleep 1
keys=$(printf '19\nl\n(sx,1)\n' | python3 -I -c '
import sys
print("".join("\\i" + ("\\n" if c == "\n" else c) for c in sys.stdin.read()), end="")')
"$C900_EMU" --firmware="$work/fw" --disk="$work/blank.img" --wire="$work/sock" \
	--max=3000000000 --input-mark='when ready' --input="$keys" \
	--stop-mark='Start address' > "$work/console" 2>&1 || :
kill $sender 2>/dev/null || :
if grep -q 'Start address' "$work/console"; then
	echo "serialcheck: loaded $(grep -ao 'Received [0-9]*K' "$work/console" | cut -d' ' -f2) over XMODEM"
else
	echo "serialcheck: FAIL"
	cat "$work/send.log" "$work/console"
	exit 1
fi
