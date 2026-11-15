# BIOS plan: hardware detection, faster RAM test, 64 KB ROM

Branch `ram-video-detection`.  Input was gathered 2026-11-12 from:

- the C900 hardware session (C900_MiSTer main 019d4ed);
- the kernel3 session;
- a survey of the alternate OSes (CP/M-8000 in commodore-900-cpm and
  c900oses: minix2, elks, EmuTOS notes);
- kboot;
- an independent review of this source.

Every OS boots the same way: ROM, then kboot, then the OS.  So most of the
benefit comes from the ROM measuring the machine once and publishing the
result in a place kboot can copy cheaply.  kboot has only a few bytes
free under its 0x4000 cap.

## 0. Ground rules

- **Keep the existing ABI working.**  CP/M and kboot call these fixed ROM
  offsets:

  | Entry   | Offset |
  |---------|--------|
  | putchar | 0x0fc2 |
  | puts    | 0x0900 |
  | getchar | 0x104a |
  | wdgo    | 0x111e |
  | wdread  | 0x11a6 |
  | ldirb   | 0x0274 |
  | pclear  | 0x0286 |
  | kbd     | 0x3f1e, 0x3f62 |

  They also use iread, dirlook, vread, jmp, segload and inb/outb, and they
  read these seg-1 cells directly:

  | Cell                  | Use                 |
  |-----------------------|---------------------|
  | 01:0000               | `romp`              |
  | 01:0614               | saved cursor        |
  | 01:041c               | keyboard init flag  |
  | 01:1236, 1240, 123a   | ROM data cells      |
  | 01:17ff               | `has_hires`         |
  | 01:1800               | `d_conhires`        |

  The display planes stay at segs 0x3A/0x3B.  Any rebuild moves code, so
  step 1 pins these locations before anything else changes.
- **Every probe follows the bus facts from the C900 session:**
  - Absent memory and I/O read 0xFFFF, with no bus error and no /WAIT.
  - A byte read of an even address returns D15:D8.  Read the SCC, CIO,
    6545 and LC at odd ports or as words.  Read HR 404H as a word.
- `make -f mk/host.mk boot-check` must keep passing.  Each phase adds its
  own emulator check.

## 1. Foundation: ABI pinning and the 64 KB build

0. **Make the source relocatable first.**  The decompilation still
   carries absolute addresses that hold only while the image is
   byte-exact.  The first code change anywhere breaks the debugger.
   - `ddt.s:1958`: command dispatch does `ldl rr2,$0x000039ba` and then
     `sub r3,$0x39ba` for its key table.
   - `ddt.s:1399`: the breakpoint sub-switch does the same with 0x31c4.
   - `ddt.s:1398`: an instruction encoded as raw `.byte`s.
   - `lkcrt.s:102-113,200`: the data-copy source, length and stack
     constants.

   Replace each with a label, then sweep with a script that relinks at a
   shifted origin and diffs the resolved addresses.  Fix this before any
   other code change.

1. **Fixed jump table** at the start of the ROM, versioned with a magic.
   Pin the existing entry offsets as stubs, so old callers keep working
   while code moves.  New services go only in the table.
2. **Pin the seg-1 data cells** listed above, using a fixed-layout
   assembly block rather than link order.
3. **64 KB image:**
   - `bootfix.c`: move TEXTMAX/DATAMAX, and stream in 16 KB chunks.  The
     1985 cc has 16-bit `int`, so the current loops break above 32 KB.
   - `prom.c`: NCHUNK 4 -> 8, giving 27256 halves.
   - `lkcrt.s:102-113,200`: replace the magic data-source, length (0x1016)
     and stack (0x121c) constants with symbols.
   - Give the stack more than its current ~518 bytes.
4. **Hardware: 64 KB is period-faithful** (C900 session, 2026-11-12).
   - The 8000HR Design Review sec 1.4 lists sockets for 23128 (32 KB) or
     27256/23256 (64 KB) parts.  Socket pin 27 already carries A14, and
     U54 asserts _ROMCS for all of segment 0.  A real board needs no
     change.
   - **The core needs** `c900_rom` AW 14 -> 15 (`c900_mainboard.sv:401-408`)
     and a 64 KB HPS loader.  That costs about 26 more M10K, but only 19
     are free, so it doesn't fit as-is.  The C900 session is deciding
     how to make room with the user.  The emulator also needs the larger
     ROM.
   - **Two builds, not runtime detection.**  The default is 64 KB, with
     data at 0xE000.  `make ROMKB=32 ROMDATA=0x6cf0` builds for a
     32 KB pair, and features that don't fit are left out of that build.
     A 64 KB image can't be put on 32 KB parts anyway.
   - **Emulator:** `commodore-900-emulator` branch `rom64` (ae91510)
     decodes 64 KB and mirrors a 32 KB pair.  CI builds the 32 KB variant
     until DEPS pins a release with it.

## 2. Hardware inventory: video

**Probe every board.**  Today's probe short-circuits: once HR is found, LR
is never tried, so the flags record a selection rather than an inventory.

**Do no LR register writes before HR is ruled out.**  On an HR machine,
any 0x04xx write with A1=0 loads U312 (mono enable and intensity).  That
includes the BIOS's LR init writes to 0x0410 and 0x0418
(`diagdisp.s:283-304`).

| Board | Probe | Publish |
|---|---|---|
| HR | Pattern 0x55AA/~ plus an anti-echo write at 0x3E0000, then the 404H word `& 3` (1 = RGBI 640x400x4, 2 = mono) | Raw 404H word.  FB base 0x3E0000, 128 KB |
| LR | 0000/FFFF at 0x370000 only once HR is absent; 6545 R14-R17 readback at 0x0413 | Present flag.  4 KB mirrored through all of seg 0x37 |
| LC (8563/8568) | ID 0xC1 at 0x01FF, R18/R19 readback through 0x0181/0x0183 with a ready timeout, as kboot `vid.c:70-133` does.  Keep NVI off and read status afterwards | ID; version from status bits 1:0 (2 = 8568) |

- **1024x800 vs 1024x1024 cannot be measured.**  The core reads 2 for
  both mono boards.  The PLEB spec value of 3 conflicts with the captured
  netlist, and U756 has not been dumped.  So publish mono only, with no
  geometry field.  kboot's `BF_HR1024` stays as the user override.
- **Selection is separate from presence.**  Console choice is the first
  of HR, LR, LC, serial unless configured otherwise (see 6.2).
- **Fix the swapped names.**
  - `romconf.has_hires` (01:17FF) is the HR bitmap console, through
    `altput`/`avidchr`.
  - `ddtvars.d_conhires` (01:1800) is the LR 6845 console, through
    `hrput`/`bvidchr`.

  The names and comments in `sio.c:9-10`, `romconf.h` and `ddtvars.h` are
  backwards, and kboot and CP/M copied the error (`romabi.h` in each).
  That is very likely CP/M bug M9.  Rename them here, and tell both
  projects.
- **`lceddt.c:133-134` sets both flags to 2** on a bad-register
  diagnostic.  Fix it.

## 3. Hardware inventory: RAM, plus the RAM test

### 3.1 Map, not extent

- **Map from the decoder layout:**
  - bank 0: segs 0x08-0x0F;
  - bank 1: segs 0x10-0x17, absent at 512 KB;
  - the RAM card: 3 MB at 0x18-0x35, or 16 MB at 0x18-0xFE.
- **Always skip segs 0x37, 0x3E and 0x3F.**  Otherwise, on a 16 MB
  machine with LR fitted, the 0x37 mirror passes and sizing runs on to
  3456K.
- **Probe each 64 KB segment independently.**  Don't stop at the first
  failure.
  - Write a unique tag per segment, then re-read every tag, to catch
    aliasing.
  - Absent memory reads 0xFFFF.
- **Never write a framebuffer segment.**
- **Publish:**
  - a list of runs `{first seg, count}` with holes and bad segments left
    out, in the shape of kboot's `bi_ram`;
  - a bad-segment bitmap;
  - the ROM's own reserved pages, so CP/M doesn't need to withhold a
    whole 64 KB page.
- **Fix the units.**  `romconf.h:7-8` says 512-byte clicks, but the code
  uses 1 KB (`rom.c memory()` shifts by 6).  Also fix the `m` command's
  offset (`rom.c:256,262` should be `(l & 0x3f) << 10`).

### 3.2 Faster test with a count-up

- **Correction: V1.0's test is linear, not quadratic.**  Each march pass
  re-reads only its own segment.  It costs about 4 word accesses per
  word, roughly 100 CPU cycles per word, plus the 1 KB sizing walk.
  That is about 9 s per MB at 6 MHz, with nothing on screen.
- **Done:** the test now runs from C, after the stack exists, one segment
  at a time with `ldir`/`cpir` (about 9 cycles per word each):
  - fill 0x5555, check, fill 0xAAAA, check, clear;
  - then a tag per segment to catch aliasing.

  That is about half V1.0's cycles, with better coverage.  The ROM's 7 KB
  data window is skipped.
- **Later, a full March C-** (10 accesses per word) as a setup-menu
  option, in the 64 KB ROM.
- **PC-style display:** `Memory test: 0000K` counts up on the console.
  - It updates every 64 KB on the active console: HR, LR, LC or serial.
  - On serial it uses CR without LF, so the line rewrites in place.
  - The final line shows the total plus `OK`, or the failing address.
- **Keep the `RAM ERROR BANK:CHIP= xx` report and the POST port 0x001d
  codes.**
  - Also print the failing physical address and the expected/read
    values.
  - `memdiag` uses a signed `div` (`ramtest.s:23-29`).  Make it unsigned,
    so 32 MB and above print correctly.
- **Quick mode:** a key (or a config bit, 6.2) skips to "size only".
  This is also the warm-reset path.

## 4. Handoff block (draft, for kernel3 and kboot review)

`romp` at 01:0000 is a POINTER.  kernel3 reads it at `commodore.c:209`,
and CP/M at `pginit.c:128`.  Today it points at the 16-byte `romconf`
at 01:17F0, and `ddtvars` sits right after it at 01:1800.  So "extend in
place" means this:

- `romp` points to a new, larger struct, wherever it lives.
- Its first 16 bytes are the legacy fields, at the same offsets.
- The legacy copies at 01:17F0..01:17FF and 01:1800 stay pinned, for
  direct-cell readers.

Readers that copy `sizeof romconf` (16 bytes) are unaffected.

```c
struct romconf {                 /* offsets in bytes */
    unsigned rom_bram;           /*  0  RAM base, 1 KB clicks */
    unsigned rom_eram;           /*  2  RAM end,  1 KB clicks */
    char    *rom_auto;           /*  4 */
    int     *rom_restart;        /*  8 */
    char     rom_fdtype;         /* 12 */
    char     rom_hdtype;         /* 13  drive type actually used */
    char     rom_ctype;          /* 14  0 = 4 MHz, 1 = 6 MHz (legacy) */
    char     has_hires;          /* 15  HR board FITTED (see below) */
    /* ---- extension: valid iff rc_magic matches ---- */
    char     rc_magic[4];        /* 16  "C9RC" */
    unsigned rc_version;         /* 20  1 */
    unsigned rc_length;          /* 22  bytes, from offset 0 */
    unsigned rc_romver;          /* 24  BCD, e.g. 0x0200 */
    unsigned rc_video;           /* 26  presence: 1 LR, 2 HR, 4 LC */
    unsigned rc_hr404;           /* 28  raw 404H word (0 if no HR) */
    char     rc_lcid, rc_lcver;  /* 30  0xC1, status & 3 */
    char     rc_console;         /* 32  BI_CON_* code */
    char     rc_conchan;         /* 33  SCC channel when serial */
    unsigned rc_serial;          /* 34  bi_serial bitmap */
    unsigned rc_nram;            /* 36 */
    struct { char seg, nseg; } rc_ram[16];   /* 38  same order as bi_ram */
    char     rc_badseg[32];      /* 70  bit per 64 KB seg */
    unsigned rc_hdcyl;           /* 102 */
    char     rc_hdheads, rc_hdsec; /* 104 */
    char     rc_bootdev[2];      /* 106 "hd"/"fd" */
    char     rc_bootunit, rc_pad;/* 108 */
    long     rc_bootoff;         /* 110 */
    char     rc_rsvd_seg;        /* 114 64 KB seg the ROM uses until the loaded program runs */
    char     rc_rsvd_nseg;       /* 115 count */
    unsigned rc_config;          /* 116 effective setup-menu bits */
    unsigned rc_cksum;           /* 118 word sum, offsets 16..rc_length-2 */
    unsigned rc_net;             /* 120 1: Am7990 LANCE fitted */
    unsigned rc_mac[3];          /* 122 LANCE address, byte 0 in the high half */
};                               /* 128 */
```

kernel3 reviewed this layout on 2026-11-12.

- **`has_hires` (01:17FF = byte 15) means "HR board fitted".**  That is
  how both readers use it: they keep seg 0x3E out of the RAM map (kboot
  `bmain.c:432`, kernel3 `commodore.c:147`).
  - The ROM's own console routing therefore moves to a new internal cell.
  - Once the setup menu can pick serial with HR fitted, 01:17FF stays 1
    while `putchar` goes to the SCC.
  - CP/M's `crsromser()` reads "both flags zero" as "the ROM programmed
    the SCC", which breaks in that case.  CP/M should move to
    `rc_console` and the init flags, and is told so.
- **The checksum covers the extension only**, offsets 16 to the word
  before `rc_cksum`.  The legacy bytes the ROM may rewrite are excluded.
- **`bi_ram` is u16 first/count** (`bootinfo.h:144-147`).  kboot masks
  the char pairs and rebuilds its 8-entry map through `mmseg`
  (`cfg.c:455-503`), so 16 runs are fine.
- **Placement:** the struct must stay inside seg 1's 7 KB window (phys
  0x0FE400+ on 512 KB), above kboot's copy limit of 0x0F3FFF.
- **Direct-cell readers:**
  - kboot `con.c:65` reads 01:17FF and 01:1800, ORed together.
  - kernel3 `console.c:116-117` reads both too.
  - Pointer readers: kernel3 `commodore.c:207-209`, and kboot `con.c:72`
    and `bmain.c:430`.
- **kboot has 952 text bytes free**, and the reader costs about 200, so
  it fits.
- Append only.  Readers judge presence by `rc_length`, as kboot does.
- Pre-change ROMs have no magic, so the current probes remain the
  fallback in kernel3 (agreed).
- Open with kboot: check the element types of `bi_ram`, and decide
  whether 16 runs is too many to copy into its 8.
- No boot-entry registers (kernel3's request).

## 5. New and fixed ROM services, through the jump table only

- **`wdgo2`:** returns the controller status, with a timeout.  kboot's
  `setgeom` waits by hand today.
- **Timing:** waits count ticks of a CIO counter, which runs from the
  fixed 6 MHz clock at any CPU speed.  The CPU clock itself is not
  measured or handed over (user decision, 2026-11-13).  Every
  hard-coded spin moves to it:
  - `wd.c:58,61`;
  - the 5M-count hard-disk retry in `rom.c:103-106`;
  - the "OI" window in `lkcrt.s:175`.

  At 48 MHz these are about 8x too short.
- **`congetc_nb`:** non-blocking console input, no echo, on every
  console.
- **`wdwrite`:** dropped (user decision, 2026-11-13).  Nothing the ROM
  boots needs it.
- **Console state at a fixed low area**, so `putchar` survives the
  kernel claiming the top 7 KB.  `rom_restart` re-entry also becomes
  possible.
- **SCC console and keyboard CIO initialised unconditionally**, with
  flags in the handoff.  Leave RTC /CS (CIO PCDD) driven.
- **SCC init:** wait for All Sent before the WR9 reset in `contab`
  (`ramtest.s:57-65,104`).  This is the garbled byte on the
  "SCC (U74) register test" line.  kernel3 fixed the same bug in 69ebb6c.

## 6. Boot and monitor

### 6.1 Loader bugs (boot.c)

1. `boot.c:143-144`: the namebuf clear skips every other byte, so a short
   path component after a long one doesn't match.
2. `boot.c:178`: `dirlook` loops on `di_size` from `buf`, which the first
   `vread` overwrites.  Directories longer than one block (32 entries)
   are effectively truncated.
3. `boot.c:316-325`: `fload` returns 512 when it ends exactly on a block,
   and then the next `ldirb(...,0)` copies 64 KB of junk.  About 1 in 512
   image sizes fail.
4. `boot.c:121,314`: read errors are ignored, and the ROM jumps into a
   corrupt image.
5. `boot.c:375-381`: past 138 blocks, `vread` silently returns zeros.  Add
   double indirect blocks, or at least an error.
6. `boot.c:321,329`: crossing a segment does `s++` instead of
   `s += 0x100`, so images over 64 KB wrap.  With 32-bit sizes and
   multi-segment loads, kboot's original reason to exist goes away,
   although kboot stays.
7. `boot.c:184-192`: a last-character mismatch still matches.
8. `boot.c:276` and `pclear`: these copy and clear 0xFFFF bytes rather
   than 0x10000.

### 6.2 Monitor and config

- **Boot menu with a calibrated countdown** and an interruptible
  autoboot, replacing the uninterruptible
  `(fd,1)coherent` → `(hd)coherent` x2 chain.
- **Config storage** in a reserved disk block: drive type, boot order,
  console choice, quick-memtest.  `S` would then survive into autoboot.
- **Fixes:**
  - `rom.c:338-368`: `FS…` formats the floppy; `format_flag` is never
    read.  This one is destructive.  Fix first.
  - `rom.c:294-319`: `gets` is unbounded.
  - `rom.c:378-388`: `P` with no unit uses an uninitialised value.
  - `porttest.c:42,68`: prints the failing segment + 1.
  - `rom.c:274-277`: `hexdigit` accepts G-Z.
  - `wd.c:98-126`: unbounded busy retry.
  - `wd.c:107`: block bits leak into the unit field.
  - `wd.c:151`: the second hard disk is never initialised.
  - `wd.c:158`: `wdinit` decodes status inconsistently.
- **Debugger:** add a word-wide `i`, so 0x0400/0x0402 and other
  even-port devices can be read.
- **Banner hygiene:** don't leave the LR cursor-0 "clear on next char"
  sentinel armed, and don't print over HR row 0.
- **Version string:** show V2.x and the build date.  `usage()` prints
  "V0.0" today.

### 6.3 Early setup menu ("press C for setup")

- **Prompt.**  As soon as the console is chosen, which is before the RAM
  test, print `Press C for setup, Space to skip memory test`.
  - Poll the keyboard and serial without blocking during the count-up and
    for a short calibrated window after it (uses 5's timer and
    `congetc_nb`).
  - The keyboard CIO must be initialised up front.  Today it is set up
    lazily, inside `getchar`.
  - Keep the old "19"/"OI" escapes working.
- **Menu.**  A plain numbered text menu that works on every console:
  - **Memory test:** full / quick (size only) / off.  Also: stop on first
    error, or continue and map bad segments.
  - **Diagnostics:** run each POST test or skip it, both the SCC/CIO/MMU
    port tests and the LC register test.
  - **Probes:** enable or disable each video board, LANCE and APU, so a
    probe that hangs on odd hardware can be turned off.
  - **Console:** auto / HR / LR / LC / serial channel, plus serial baud.
  - **Display:** HR mono geometry, 1024x800 or 1024x1024.  This feeds
    `BF_HR1024` until the hardware can report it.
  - **Disk:** hard-disk type per unit (replaces the `S` that never
    reaches autoboot); boot order and default boot string; countdown
    length.
  - **Network:** see 7.1.
  - **Boot with these settings / discard.**  Settings are not saved; see
    Storage.
- **The ROM never writes config to disk.**  Original installs have no
  reserved area, and the RTC (OKI MSM58321) has no user RAM.
  - **Menu changes last for the current boot only.**
  - **Defaults live in a config block in the ROM image** at a fixed
    offset, with a magic and checksum.  A host tool `mk/romcfg` edits it.
    - On MiSTer, that means the ROM file on SD.
    - On a real board, it means reburning the EPROM.
  - **A stock image uses the stock defaults**, so behaviour on existing
    machines is unchanged.
  - **Later, optionally:** read `/etc/bios.cfg` through the existing fs
    code.  It is read-only, and a missing file means defaults.  It can
    only set boot-stage options (boot order, default boot string,
    countdown), since it is read after POST.
  - A bad checksum means defaults are used, and a message says so.
  - Holding a key at reset, or a jumper, forces defaults, so a bad
    config can't brick the boot.
- **Handoff.**  The effective settings go into the handoff block (a
  `rc_config` word plus net fields), so kboot and the OS see what was
  skipped or chosen.

## 7. Later or optional

### 7.1 Network configuration and net boot

There are two links, but only one stack.  Above the link layer it's the
same small UDP/IP + TFTP code:

- **SLIP on an SCC channel.**  Works on every machine today; the core
  already has SLIP work.  Slow, but a good first target and easy to test
  in the emulator.
- **Ethernet on the Am7990 LANCE.**
  - Detect it: RAP 0x0702 = 0, then CSR0 has STOP set.
  - The MAC is in the PROM at 0x0710-0x0714.
  - On core main the card is a stub; the real one is on the `lance`
    branch, behind OSD P1O[8].

Scope, kept small enough for a ROM:

- ARP, IPv4, UDP, and BOOTP or DHCP-lite (DISCOVER/OFFER/REQUEST/ACK).
  Fallback: static settings from the setup menu (IP, mask, gateway,
  server, file name).
- TFTP read only: 512-byte blocks, with retry and timeout from the
  calibrated timer.
- **The file loaded is kboot or an l.out kernel.**  It goes through the
  same loader as disk, so the multi-segment loader fixes in 6.1 come
  first.
- **New boot string:** `(net)file` and `(sl,ch)file`, alongside
  `(hd)`/`(fd)`, and selectable in the boot order.
- **Handoff:** IP, mask, gateway, server, MAC and the link used, so the
  OS can skip its own DHCP.
- **Size estimate:**
  - 6-10 KB of code.
  - LANCE ring buffers and a buffer of about 1.5 KB each go in RAM the
    ROM reserves and reports.

  This only fits in the 64 KB ROM, so it depends on 1.4.
- **LANCE DMA facts** (C900 session, 2026-11-12):
  - DMA uses 24-bit physical addresses and bypasses the MMU.  Any RAM
    segment is reachable; ROM and framebuffer segments are not usable.
    Rings may cross the 0x0FFFFF/0x100000 boundary.
  - The init block is 12 words, word-aligned.
  - Descriptors are 4 words on a quadword boundary, and ring length is
    2^LEN.
  - Buffers can have any alignment.
  - Program ACON, BCON and BSWP to 1 (AMD TM sec 3.3).  Init-block and
    descriptor words are ordinary Z8000 words.  PADR (MAC) and LADRF
    are byte strings, so byte-swap each of their words.
### 7.2 Serial boot

There are two levels.  Both go through the same loader as disk, so a
kboot or l.out image can be sent.

- **Level 1, raw download.**  `(sx,ch)` boots over XMODEM-CRC or
  XMODEM-1K into RAM, then jumps.  A monitor `x addr` command loads raw
  data, and `g addr` jumps.
  - Under 1 KB of code, so it fits in 32 KB too.
  - The host side is any terminal program, or a small script that drives
    the emulator or MiSTer UART.
- **Level 2, SLIP + TFTP**, as 7.1, for a server-driven boot that shares
  the network code.
- **Speed.**  The SCC uses the fixed 6 MHz PCLK in x16 mode.
  - TC 0x11 = 9600 baud, about 870 B/s at 8N2, so a 64 KB image takes
    about 75 s.
  - TC 8 gives 18750 baud (-2.3% from 19200), and TC 3 gives 37500
    (-2.3% from 38400), both normally inside UART tolerance.
  - The boot command picks the rate temporarily, and the setup menu can
    save it.
  - Hardware flow control (RTS/CTS on the SCC) avoids overruns with
    polled reception at high rates.
- **Today's ROM can't do it.**  There is no download command, and
  entering bytes by hand in `ddt` isn't practical.

### 7.3 Built-in terminal (`t` command, and a setup-menu entry)

The C900 becomes a dumb terminal to another machine on any SCC channel.

- **Pass-through:** the console keyboard goes to the SCC channel, and the
  channel goes to the console screen.  Works from HR, LR, LC or a serial
  console (channel A to channel B as a bridge).
- **Options:** channel, baud (the 7.2 rates), 8N1/7E1, local echo, and
  CR to CR/LF on receive and on send.
- **Escape key `Ctrl-]`** opens a one-line menu:
  - `q` quit to the monitor;
  - `b` change baud;
  - `e` toggle echo;
  - `r` receive a file by XMODEM into RAM (7.2), then `g` to run it;
  - `s` send a RAM range by XMODEM;
  - `^]` send a literal `^]`.

  So the terminal is also the user interface for serial boot.
- **Glass TTY first:** CR, LF, BS, TAB, BEL and FF, as the ROM renderers
  already do.
  - Optional later: a VT52 subset (ESC A/B/C/D/H/J/K/Y), which costs
    little and is enough for full-screen editors.  It goes in the shared
    renderer, so the boot console gains it too.
- **The speed hazard.**  Each LR character re-enters the ROM, costing
  about 6-20K T-states (about 3 ms at 6 MHz), while 9600 baud delivers a
  byte every 1 ms.
  - Receive goes into a ring buffer of about 2 KB, filled by a
    non-blocking poll between characters drawn, or by the SCC receive
    interrupt.
  - RTS is dropped when the buffer is three-quarters full.
  - At 48 MHz the problem mostly disappears, but the buffer makes it
    safe at any clock.
- **Size:** about 1-1.5 KB without VT52.  Fits in 32 KB if needed.  It
  depends on `congetc_nb` and the timer (5).
- **APU (Z8070) detect:** unverified.  The idea is that with EPA set, an
  EPU read gives 0xFFFF when no APU is fitted.  Confirm with the C900
  session first.
- **ROM checksum self-test.**
- **Disk label and partition table on cylinder 0.**

## 8. Further suggestions

### 8.1 Robustness

- **Warm vs cold reset.**
  - Mark warm boots with a magic in the ROM's seg-1 area.
  - On a warm reset, skip the destructive RAM test and keep RAM intact.
  - Offer "save memory" through the setup menu: XMODEM out over serial
    (7.2), or raw to a disk area.  That gives a post-mortem crash dump of
    a hung kernel.
  - The dump only works because the memory test (3.2) can be skipped
    entirely.
- **Trap reporting.**  On an unexpected trap, `trap.c` drops straight
  into `ddt` with nothing printed.  Print the trap name, PC (seg:off),
  FCW and the MMU violation status first.  For a kernel that dies early
  that's often all the evidence there is.
- **Stack guard.**  The ROM stack is about 518 bytes in a 7 KB window
  (`lkcrt.s:200`), and nothing checks it.
  - Place a canary word below it, and check it in the monitor loop and on
    traps.
  - Report "ROM stack overflow" instead of corrupting `romconf`.
- **ROM self-check.**  A checksum word at a fixed offset, verified at
  POST (POST code on 0x001d), plus the 32/64 KB part detection from 1.4.
- **Diagnostic failures shouldn't block boot.**  `abflags[0]` doubles as
  the burn-in flag, so one failed test suppresses the rest and stops
  autoboot.
  - Separate the two.
  - After a failure, run the remaining tests, print a summary, and offer
    "continue boot" with a countdown.
- **Leftover review items:**
  - `diagdisp.s:371`: `ldb rl1,0x0020` loads ROM byte 0x20, not `$0x20`.
    It works only because that byte happens to be zero.
  - `lceddt.c:50-51,91-93`: registers 0x4d and 0xcd are tested twice.
  - `lceddt.c:180`: the table length is the literal 84, not `sizeof`.
  - `boot.c:257`: rounds a size that is already 1 KB-aligned up by
    another 1 KB.

### 8.2 Monitor usability

- **`ls (hd)/dir`:** a directory listing, built on the fixed `dirlook`.
  You see what is bootable instead of guessing names.
- **Fallback kernel.**  If `(hd)coherent` fails, try `(hd)coherent.old`
  and then `(hd)kboot`.  The list is configurable in setup.
- **Line editing:**
  - DEL and BS both erase, and `^U` kills the line, replacing `@`, which
    also makes `@` usable in paths;
  - one-line history, so up-arrow or `^P` recalls the last boot command;
  - a bounded buffer (fixes `rom.c:294`).
- **Merged help.**  One `?` covers monitor and `ddt` commands, with the
  ROM version, build date, part size and the detected hardware summary
  (an "inventory" screen).
- **New `ddt` commands:**
  - `iw`/`ow` for word I/O at even ports;
  - `d addr len`, a hex+ASCII dump;
  - `x`, a memory search for a byte or word pattern.
- **Disk tools:**
  - `r unit blk addr` and `w unit blk addr` for raw sector read and
    write;
  - a surface scan that lists bad blocks;
  - `fd` to `hd` copy of a block range, for installing from floppy.

  They need `wdwrite` (5).

### 8.3 Hardware coverage

- **Mouse and keyboard test** in diagnostics: show the HR mouse X and
  buttons from 404H/406H live, and keycodes as pressed.
- **Video test patterns** per board: a grid, all-on, and a character set.
  They're handy for MiSTer scaler checks, and quick to add once the
  inventory exists.
- **APU (Z8070) detect and a small FP self-test** once the C900 session
  confirms the EPA probe.
- **CPU speed shown at POST**, `CPU: Z8001 48 MHz`, from the 5
  calibration.
- **Serial autobaud** on the console channel: wait for CR and measure the
  bit time against the CIO counter.  Optional, and only when no setup
  config exists.

### 8.4 Process and cross-project

- **One source of truth for the ROM ABI.**  The swapped console-flag
  names arose because kboot, CP/M and kernel3 each hand-copied
  `romabi.h`.
  - Generate `romabi.h` from this build: jump-table offsets, seg-1 cells
    and the handoff struct, from `nm` plus a template.
  - Publish it as a build artifact.
  - Have the other repositories check their copy against it in CI.
- **Emulator regression tests per feature.**  The existing `boot-check`
  only shows the ROM reaches the boot block.  Add scripted runs for:
  - RAM configurations (512K, 1M, 3M, 16M, with an LR mirror);
  - each video board;
  - a planted bad segment;
  - each setup-menu path;
  - XMODEM boot.
- **Build both 32 KB and 64 KB images** on every CI run, with a size
  report per section so creep is visible.
- **Tags:** keep the `1.0` tag byte-exact.  New releases are
  `2.0-alphaN`, and the version is printed in the banner and stored in
  `rc_romver`.

## Order of work

0. **Relocatable source (1.0): the debugger's hard-coded table
   addresses.**  Add the shifted-origin relink check, plus CI 32/64 KB
   builds and the generated `romabi.h` (8.4).
1. **Destructive and correctness fixes that need no layout change:**
   - `FS` formats;
   - namebuf clear;
   - `dirlook`;
   - `fload` count 0;
   - error checks;
   - the units comment and the `m` offset;
   - the swapped flag names (rename only).
2. **ABI pinning (jump table, fixed data cells) and the 64 KB build**,
   after the hardware decode is confirmed.  Until then, use the 32 KB
   build.
3. **The calibrated timer**, which the RAM test, autoboot and `wdgo2` all
   need.
4. **The RAM map, plus the faster test with the count-up.**  The test
   and count-up are done; the map is not.
5. **The video inventory**, including the LR-write fix on HR machines.
6. **The handoff block.**  Send the final layout to kernel3 and kboot.
   kboot copies it into bootinfo v5/v6.
7. **New services:** `wdgo2`, `congetc_nb`, `wdwrite`, the low console
   state.
8. **The setup menu (per-boot), the ROM config block plus `romcfg`
   (6.3), then the boot menu and the loader upgrades.**
9. **XMODEM serial boot (7.2): done, as `(sx,n)`.  Terminal (7.3)
   later.**  They're cheap, and
   useful for bring-up as soon as `congetc_nb` exists.
10. **Net boot:** SLIP first, then LANCE once the card is on core main
    (7.1).

## Open questions

- **The 64 KB ROM in the core is decided** (2026-11-12).  First try: serve
  the ROM from SDRAM, provided 6 MHz ROM timing stays exact.  Otherwise,
  find M10K savings.
  - Built on C900_MiSTer `rom64` (ea63f4c), not yet merged or tested on
    hardware.  The ROM is in SDRAM with 6 MHz timing unchanged.
  - boot.rom is 48 KB (a 32 KB pair, mirrored at 0x8000) or 128 KB (a
    27256 pair at 0x10000, giving a full 64 KB).
  - The emulator still has a 32 KB ROM.
- What value does 404H give on a real 1024x1024 board?  This needs a U756
  dump.  Until then, `BF_HR1024` stays.
- **Multiple display cards.**  The core allows exactly one.  On a real
  board, LR + HR conflict in I/O decode (HR's U755 claims all of 0x04xx),
  so treat that pair as unsupported.  Probe HR first, and if it is
  present, never touch LR registers.  LC doesn't overlap either, but it
  was never built, so a combination is untested.
- **RTC NVRAM and LANCE DMA: answered.**  There is no NVRAM, and the
  user rejected disk writes, so defaults live in a ROM-image config block
  and menu changes are per-boot (6.3).  LANCE DMA is covered in 7.1.
- What is the `bi_ram` element type, and the run-count limit?  (kboot.)
