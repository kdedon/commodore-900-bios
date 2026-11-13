#!/usr/bin/env python3
"""xsend -- send a file by XMODEM-CRC to an emulator serial port.

    xsend.py SOCKET FILE [128|1024]

Listens on the AF_UNIX socket SOCKET; start the emulator with
--wire=SOCKET (or --wireN=SOCKET) and boot (sx,N) on the machine.
"""
import os
import socket
import sys


def crc16(d):
    crc = 0
    for x in d:
        crc ^= x << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xffff
    return crc


def main():
    path, name = sys.argv[1], sys.argv[2]
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 1024
    data = open(name, 'rb').read()
    if os.path.exists(path):
        os.unlink(path)
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(path)
    srv.listen(1)
    con, _ = srv.accept()

    def get():
        b = con.recv(1)
        if not b:
            sys.exit('xsend: connection closed')
        return b[0]

    while True:
        start = get()
        if start in (ord('C'), 0x15):
            break
    crc = start == ord('C')
    blk = pos = tries = 0
    while pos < len(data):
        chunk = data[pos:pos + size].ljust(size, b'\x1a')
        num = (blk + 1) & 0xff
        pkt = bytes([0x02 if size == 1024 else 0x01, num, 255 - num]) + chunk
        pkt += crc16(chunk).to_bytes(2, 'big') if crc else bytes([sum(chunk) & 0xff])
        con.sendall(pkt)
        r = get()
        while r not in (0x06, 0x15, 0x18):
            r = get()
        if r == 0x18:
            sys.exit('xsend: cancelled by the receiver')
        if r == 0x06:
            pos += size
            blk += 1
            tries = 0
        else:
            tries += 1
            if tries > 10:
                sys.exit('xsend: too many retries')
    con.sendall(b'\x04')
    while get() != 0x06:
        con.sendall(b'\x04')
    print('xsend: %d bytes in %d blocks' % (len(data), blk), file=sys.stderr)


if __name__ == '__main__':
    main()
