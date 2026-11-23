# Commodore 900 BIOS

This repository is intended to capture work on reverse engineering and
modification of the Commodore 900 BIOS.

## Building

The BIOS is built with the original Mark Williams COHERENT toolchain
(`cc` / `as` / `nld`).  That is not a nostalgia choice: byte-exactness
requires it.  Our own Z8001 compiler is a different compiler and emits
different -- correct, but different -- code.

Both ways round use the same `Makefile`, so a host build and a native
build are the same build.

### On the C900

    make rom        # compile, link, flatten to the 32 KB ROM image
    make split      # -> rom.H (even bytes) + rom.L (odd bytes)

### On a host

Every 1985 tool is a Z8001 COHERENT binary, so a host build runs each one
as a guest process under the emulator's `c900 --exec`.  `mk/host.mk` is
the entry point; `mk/mwc1985/` holds one wrapper per tool.

    make -f mk/host.mk deps      # place the emulator DEPS names
    make -f mk/host.mk stage     # build the guest root from vendor/
    make -f mk/host.mk split     # rom.H + rom.L
    make -f mk/host.mk boot-check            # boot what was built
    make -f mk/host.mk check C900_ROMREF=…   # the comparison below

The 1985 compiler is IN this repository, at `vendor/mwc-1985`: the
preprocessor and the three passes, `as`, `nld`, `nm`, `size`, the C
library, the C runtime startoff and the `/usr/include`, `/usr/sys/h` and
`/usr/sys/z8001/h` header trees, laid out at the guest paths this
`Makefile` names them by.  They are recovered Z8001 binaries and exist
nowhere else, so they are tracked rather than fetched --
`vendor/mwc-1985/SOURCES.md` has the provenance, the hashes and why a
repository that holds no other binaries holds these.

`DEPS` therefore names ONE edge, the emulator: every 1985 tool is a
Z8001 binary and `c900 --exec` is what runs one.  `make deps` places it,
and `mk/deps.sh` refuses by name, with the paths it tried, when it is
missing.  Nothing else is fetched, and no checkout of any other
repository is consulted.

Setting `C900_COH_IMAGE` stages the library and the headers out of a
stock COHERENT 0.7.3 hard-disk image instead, which is how the vendored
copies are checked against a real disk.  That image is in no repository;
`vendor/` is the path that works anywhere.

`vendor/mwc-1985/usr/sys/h/con.h` makes this repository impossible to
check out on Windows -- `con` is a reserved DOS device name and git
fails with "invalid path".  That is accepted rather than worked around:
the name is what the reproduced sources `#include`, CI is Linux, and no
Windows binary is published.  Build in WSL or a container.

## Defaults

Press C during the power-on tests for a setup menu; Space skips the
memory test.  Menu choices last for one boot.  The defaults it starts
from live in the last 16 bytes of a 64 KB image and are set on a host:

    python3 mk/romcfg.py rom.H rom.L                  # show
    python3 mk/romcfg.py rom.H rom.L --memtest skip --boot hd --disk 2
    python3 mk/romcfg.py rom.H rom.L --reset

`--boot` is `fd-hd`, `hd`, `fd` or `monitor`; `--wait` is how long, in
tenths of a second, the ROM waits for C after the tests.

## ROM calls

Programs call the ROM through the service table at 00:029E, magic
`C9JT`; `make -f mk/host.mk abi-check` writes its header,
`build/romabi.h`.  The 64 KB image also keeps the V1.0 entry points:

| Routine | Offset |
|---------|--------|
| puts    | 0x0900 |
| putchar | 0x0fc2 |
| getchar | 0x104a |
| wdgo    | 0x111e |
| wdread  | 0x11a6 |

The 32 KB image has no room for them: use the table.

## Serial boot

`(sx,n)` at the monitor prompt receives a load module by XMODEM (CRC or
checksum, 128- or 1024-byte blocks) on serial port n -- 0 is the console
port, 1 the motherboard's channel A, 2-5 the lo-res board's -- at 9600
baud, then boots it as if from disk.  It may be up to 192 KB.  Any
terminal program can send it; under the emulator, `mk/xsend.py SOCKET
FILE` sends to a port attached with `--wire=SOCKET`.

## Network boot

`(sl,n)file` fetches a load module by TFTP over SLIP on serial port n
and boots it.  The machine is 10.0.9.2 and the server 10.0.9.1;
`(sl,n)a.b.c.d:file` names the server, and the machine takes the next
address up.  On a Unix host, `slattach` and a TFTP server will do;
under the emulator, `mk/tftpslip.py SOCKET DIR` serves DIR to a port
attached with `--wire=SOCKET`.

`(le)[server:]file` boots a file over the LANCE card through the card's
own boot ROM, which the ROM finds in the card's window, copies to RAM
and starts at power-on.  The card ROM does the networking; `n` shows
its address and configuration.  `romcfg.py --cardrom off`, or setup
item 4, leaves the card ROM alone.

## Terminal

`t [n]` at the monitor prompt connects the console to serial port n
(default 1) as a dumb terminal at 9600 baud.  Ctrl-] then q quits, e
toggles local echo, b steps the rate (9600, 18750, 37500), and a second
Ctrl-] sends one.

## It started byte exact

At the 1.0 tag this source, built with the original passes, reproduced
Commodore's shipped V1.0 EPROMs exactly:

    372820dbc2e5dca437599febae482807  rom.H == boot-H_V_1.0.bin
    3b012295da0f72be7b6eb4c2a28c80e4  rom.L == boot-L_V_1.0.bin

All 19 link objects matched as well.  That is the evidence the
disassembly is faithful, and it is recorded here rather than tested for:
two of this repository's goals are to CHANGE the ROM, so after 1.0 a
build matching the reference dumps would mean the change had not been
made.  `make -f mk/host.mk check` still performs the comparison for
anyone rebuilding the 1.0 tag.

The test that outlives it is `make -f mk/host.mk boot-check`: boot the
ROM just built in the emulator and require it to run every diagnostic, in
order, and hand over to the boot block -- the same point the reference
ROM reaches.  That tests the ROM rather than a hash of it, and it keeps
working when the bytes deliberately change.

## Goals

The efforts captured by this repository include:

1. A byte exact decompilation of the BIOS, allowing for recreation of
the shipping BIOS. 
1. Extensions to the BIOS, such as more hard drive layouts.
1. Expansion of the BIOS to 64K.

## AI Disclosure

The initial decompilation of the BIOS was done with support using AI
tooling.
