#!/usr/bin/env python3
"""romcfg -- show or set the boot ROM's defaults in a rom.H/rom.L pair.

    romcfg.py rom.H rom.L                       show
    romcfg.py rom.H rom.L --memtest skip --boot hd --disk 2 --wait 20
    romcfg.py rom.H rom.L --reset               back to the built-in defaults

The defaults are 16 bytes at the end of a 64 KB image: "C9DF", version 1,
memory test (0 full, 1 skip), boot order (0 floppy then hard disk, 1 hard
disk, 2 floppy, 3 monitor), hard disk type 0-3, wait for C in tenths of a
second, network card ROM (0 on, 1 off), and a checksum word that makes the 16-bit words sum to 0.
"""
import argparse
import sys

OFF = 0xfff0
BOOT = ['fd-hd', 'hd', 'fd', 'monitor']
DISKS = ['2-head 10MB', '4-head 10MB', '4-head 20MB', '7-head 42MB']


def load(h, l):
    hb, lb = open(h, 'rb').read(), open(l, 'rb').read()
    if len(hb) != 0x8000 or len(lb) != 0x8000:
        sys.exit('romcfg: need the halves of a 64 KB image (32 KB each)')
    return bytearray(b for pair in zip(hb, lb) for b in pair)


def save(img, h, l):
    open(h, 'wb').write(bytes(img[0::2]))
    open(l, 'wb').write(bytes(img[1::2]))


def words(b):
    return [b[i] << 8 | b[i + 1] for i in range(0, len(b), 2)]


def valid(blk):
    return blk[:4] == b'C9DF' and blk[4] == 1 and sum(words(blk)) & 0xffff == 0


def show(blk):
    if not valid(blk):
        print('built-in defaults: full memory test, boot fd-hd, disk 2 (4-head 20MB), wait 1.0 s, card ROM on')
        return
    print('memory test: %s' % ('skip' if blk[5] else 'full'))
    print('boot:        %s' % BOOT[blk[6]] if blk[6] < 4 else 'boot:        ?')
    print('disk:        %d (%s)' % (blk[7], DISKS[blk[7]]) if blk[7] < 4 else 'disk:        ?')
    print('wait for C:  %.1f s' % ((blk[8] or 10) / 10))
    print('card ROM:    %s' % ('off' if blk[9] else 'on'))


def main():
    ap = argparse.ArgumentParser(description='Show or set the boot ROM defaults.')
    ap.add_argument('high', help='rom.H, the even bytes')
    ap.add_argument('low', help='rom.L, the odd bytes')
    ap.add_argument('--memtest', choices=['full', 'skip'])
    ap.add_argument('--boot', choices=BOOT)
    ap.add_argument('--disk', type=int, choices=range(4))
    ap.add_argument('--wait', type=int, help='tenths of a second, 1-255')
    ap.add_argument('--cardrom', choices=['on', 'off'], help='run the network card ROM')
    ap.add_argument('--reset', action='store_true', help='remove the block')
    a = ap.parse_args()

    img = load(a.high, a.low)
    blk = img[OFF:OFF + 16]
    if a.reset:
        img[OFF:OFF + 16] = bytes(16)
        save(img, a.high, a.low)
        show(bytes(16))
        return
    if not any(v is not None for v in (a.memtest, a.boot, a.disk, a.wait, a.cardrom)):
        show(blk)
        return
    if not valid(blk):
        blk = bytearray(b'C9DF\x01\x00\x00\x02\x0a' + bytes(7))
    if a.memtest:
        blk[5] = a.memtest == 'skip'
    if a.boot:
        blk[6] = BOOT.index(a.boot)
    if a.disk is not None:
        blk[7] = a.disk
    if a.wait is not None:
        if not 1 <= a.wait <= 255:
            sys.exit('romcfg: --wait is 1-255 tenths')
        blk[8] = a.wait
    if a.cardrom:
        blk[9] = a.cardrom == 'off'
    blk[14:16] = b'\0\0'
    ck = -sum(words(blk)) & 0xffff
    blk[14:16] = bytes([ck >> 8, ck & 0xff])
    img[OFF:OFF + 16] = blk
    save(img, a.high, a.low)
    show(blk)


if __name__ == '__main__':
    main()
