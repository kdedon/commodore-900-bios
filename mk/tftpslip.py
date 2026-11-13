#!/usr/bin/env python3
"""tftpslip -- a TFTP server on a SLIP link, for booting an emulator.

    tftpslip.py SOCKET DIR [--server 10.0.9.1]

Listens on the AF_UNIX socket SOCKET; start the emulator with
--wire=SOCKET (or --wireN=SOCKET) and boot (sl,N)file on the machine.
Files are served read-only from DIR.
"""
import argparse
import os
import socket
import struct
import sys

END, ESC = 0xc0, 0xdb


def csum(b):
    if len(b) & 1:
        b += b'\0'
    s = sum(struct.unpack('>%dH' % (len(b) // 2), b))
    while s >> 16:
        s = (s & 0xffff) + (s >> 16)
    return ~s & 0xffff


class Link:
    def __init__(self, con):
        self.con, self.buf = con, b''
        self.con.settimeout(2.0)

    def frame(self):
        """One SLIP frame, or None on a 2 s timeout."""
        out, esc = bytearray(), False
        while True:
            if not self.buf:
                try:
                    self.buf = self.con.recv(4096)
                except socket.timeout:
                    return None
                if not self.buf:
                    sys.exit('tftpslip: connection closed')
            c, self.buf = self.buf[0], self.buf[1:]
            if c == END:
                if out:
                    return bytes(out)
                continue
            if c == ESC:
                esc = True
                continue
            if esc:
                c = {0xdc: END, 0xdd: ESC}.get(c, c)
                esc = False
            out.append(c)

    def send(self, b):
        b = b.replace(b'\xdb', b'\xdb\xdd').replace(b'\xc0', b'\xdb\xdc')
        self.con.sendall(b'\xc0' + b + b'\xc0')


def udp(src, dst, sport, dport, data):
    ulen = 8 + len(data)
    pseudo = src + dst + struct.pack('>BBH', 0, 17, ulen)
    u = struct.pack('>HHHH', sport, dport, ulen, 0) + data
    u = u[:6] + struct.pack('>H', csum(pseudo + u) or 0xffff) + u[8:]
    ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, 20 + ulen, 0, 0, 64, 17, 0, src, dst)
    ip = ip[:10] + struct.pack('>H', csum(ip)) + ip[12:]
    return ip + u


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('socket')
    ap.add_argument('dir')
    ap.add_argument('--server', default='10.0.9.1')
    a = ap.parse_args()
    me = socket.inet_aton(a.server)
    if os.path.exists(a.socket):
        os.unlink(a.socket)
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(a.socket)
    srv.listen(1)
    link = Link(srv.accept()[0])
    while True:
        f = link.frame()
        if f is None or len(f) < 28 or f[9] != 17:
            continue
        peer, cport, dport = f[12:16], *struct.unpack('>HH', f[20:24])
        op = struct.unpack('>H', f[28:30])[0]
        if dport != 69 or op != 1:
            continue
        name = f[30:].split(b'\0')[0].decode()
        path = os.path.join(a.dir, os.path.basename(name))
        if not os.path.isfile(path):
            link.send(udp(me, peer, 4000, cport, b'\0\x05\0\x01not found\0'))
            continue
        data = open(path, 'rb').read()
        blk = 1
        while True:
            chunk = data[(blk - 1) * 512:blk * 512]
            pkt = udp(me, peer, 4000, cport, struct.pack('>HH', 3, blk & 0xffff) + chunk)
            for _ in range(10):
                link.send(pkt)
                r = link.frame()
                if r and len(r) >= 32 and struct.unpack('>HH', r[28:32]) == (4, blk & 0xffff):
                    break
            else:
                sys.exit('tftpslip: no ACK for block %d' % blk)
            if len(chunk) < 512:
                print('tftpslip: sent %s, %d bytes' % (name, len(data)), file=sys.stderr)
                break
            blk += 1


if __name__ == '__main__':
    main()
