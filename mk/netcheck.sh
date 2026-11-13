#!/bin/sh
# mk/netcheck.sh -- boot over the network: serve the ROM's own load module
# by TFTP over SLIP to (sl,1) in load-only mode and require the loader to
# accept it.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
C900_ROOT=$root
. "$root/mk/emulator.sh"
emu_need "boot over the network"

work=$root/build/net
rm -rf "$work"
mkdir -p "$work/fw" "$work/srv"
cp "$root/rom.H" "$work/fw/bios_h.bin"
cp "$root/rom.L" "$work/fw/bios_l.bin"
cp "$root/rom.out" "$work/srv/rom.out"
truncate -s 10M "$work/blank.img"
python3 -I "$here/tftpslip.py" "$work/sock" "$work/srv" > "$work/srv.log" 2>&1 &
server=$!
sleep 1
keys=$(printf '19\nl\n(sl,1)rom.out\n' | python3 -I -c '
import sys
print("".join("\\i" + ("\\n" if c == "\n" else c) for c in sys.stdin.read()), end="")')
"$C900_EMU" --firmware="$work/fw" --disk="$work/blank.img" --wire="$work/sock" \
	--max=3000000000 --input-mark='when ready' --input="$keys" \
	--stop-mark='Start address' > "$work/console" 2>&1 || :
kill $server 2>/dev/null || :
if grep -q 'Start address' "$work/console"; then
	echo "netcheck: loaded $(grep -ao 'Received [0-9]*K' "$work/console" | cut -d' ' -f2) by TFTP over SLIP"
else
	echo "netcheck: FAIL"
	cat "$work/srv.log" "$work/console"
	exit 1
fi
