/*
 * Commodore M-series boot ROM.
 * Network boot: (sl,n)[server:]file fetches a load module by TFTP over
 * SLIP on serial port n into the serial boot buffer, then the disk loader
 * runs it.  The machine is 10.0.9.2 and the server 10.0.9.1 unless a
 * server is named, when the machine takes the next address up.
 *
 * Frames are taken apart as they arrive: the IP, UDP and TFTP headers go
 * to pkt, the file data straight to its place in the buffer.
 */
#if ROMKB > 32
#include "rom.h"

#define END     0xc0            /* SLIP */
#define ESC     0xdb
#define XSEGS   3

#define MYPORT  1069
#define BLKMAX  ((XSEGS * 65536L) / 512)

extern int    rx();
extern        tx();
extern int    sport();
extern long   mkfp();
extern char   badport[];
extern unsigned xseg;
extern long   xlen;

static unsigned char me[4] = { 10, 0, 9, 2 };
static unsigned char srv[4] = { 10, 0, 9, 1 };
static unsigned char pkt[32];   /* IP, UDP and TFTP headers received */
static unsigned char out[100];  /* packet being sent */
static int      outlen;
static unsigned tid;            /* server's port for this transfer */
static unsigned blk;            /* block expected next */

static unsigned
w16(p)
register unsigned char *p;
{
        return (p[0] << 8 | p[1]);
}

static
put16(p, v)
register unsigned char *p;
unsigned v;
{
        p[0] = v >> 8;
        p[1] = v;
}

/* One's complement sum of n bytes, added to sum. */
static long
cksum(sum, p, n)
long sum;
register unsigned char *p;
register int n;
{
        for (; n > 1; n -= 2, p += 2)
                sum += w16(p);
        if (n)
                sum += (unsigned)*p << 8;
        return (sum);
}

static unsigned
fold(sum)
long sum;
{
        while (sum >> 16)
                sum = (sum & 0xffffL) + (sum >> 16);
        return (~(unsigned)sum & 0xffff);
}

static
resend()
{
        register int i, c;

        tx(END);
        for (i = 0; i < outlen; i++) {
                c = out[i];
                if (c == END) {
                        tx(ESC);
                        c = 0xdc;
                } else if (c == ESC) {
                        tx(ESC);
                        c = 0xdd;
                }
                tx(c);
        }
        tx(END);
}

/*
 * Wrap the n-byte UDP payload already at out+28 to the server's port
 * and send it.
 */
static
send(port, n)
unsigned port;
int n;
{
        register int i;

        outlen = 28 + n;
        out[0] = 0x45;
        out[1] = 0;
        put16(out + 2, outlen);
        put16(out + 4, 0);
        put16(out + 6, 0);
        out[8] = 64;
        out[9] = 17;
        put16(out + 10, 0);
        for (i = 0; i < 4; i++) {
                out[12 + i] = me[i];
                out[16 + i] = srv[i];
        }
        put16(out + 10, fold(cksum(0L, out, 20)));
        put16(out + 20, MYPORT);
        put16(out + 22, port);
        put16(out + 24, 8 + n);
        put16(out + 26, 0);             /* no UDP checksum */
        resend();
}

/*
 * Receive one frame.  Data of the TFTP block expected next is written to
 * the buffer as it comes.  Returns the frame length, 0 for a
 * frame that is not a good UDP datagram to us, or -1 if the line goes
 * quiet for 3 s.
 */
static
frame()
{
        register int c, n;
        int esc;
        long sum, at;

        n = 0;
        esc = 0;
        sum = 0;
        at = -1;
        for (;;) {
                if ((c = rx(300)) < 0)
                        return (-1);
                if (c == END) {
                        if (n == 0)
                                continue;
                        break;
                }
                if (c == ESC) {
                        esc = 1;
                        continue;
                }
                if (esc) {
                        c = c == 0xdc ? END : c == 0xdd ? ESC : c;
                        esc = 0;
                }
                if (n < 32)
                        pkt[n] = c;
                if (n == 32 && pkt[9] == 17 && w16(pkt + 28) == 3
                    && w16(pkt + 30) == blk)
                        at = (long)(blk - 1) << 9;
                if (n >= 32 && at >= 0 && n < 32 + 512)
                        *(char *)mkfp((xseg + (unsigned)(at >> 16)) << 8,
                            (unsigned)at) = c, at++;
                if (n >= 20)
                        sum += n & 1 ? c : (unsigned)c << 8;
                n++;
        }
        if (n < 32 || pkt[0] != 0x45 || pkt[9] != 17 || w16(pkt + 2) != n
            || fold(cksum(0L, pkt, 20)) != 0
            || pkt[16] != me[0] || pkt[17] != me[1] || pkt[18] != me[2]
            || pkt[19] != me[3] || w16(pkt + 22) != MYPORT)
                return (0);
        if (w16(pkt + 26) != 0) {
                sum = cksum(sum, pkt + 12, 8);
                sum += 17 + w16(pkt + 24);
                if (fold(sum) != 0)
                        return (0);
        }
        return (n);
}

/*
 * Dotted quad at *sp followed by ':' into a, or leave both alone.
 */
static
quad(sp, a)
char **sp;
unsigned char *a;
{
        register char *s;
        register int i, v;
        unsigned char q[4];

        s = *sp;
        for (i = 0; i < 4; i++) {
                for (v = 0; *s >= '0' && *s <= '9'; s++)
                        v = v * 10 + *s - '0';
                q[i] = v;
                if (*s++ != (i < 3 ? '.' : ':'))
                        return;
        }
        for (i = 0; i < 4; i++)
                a[i] = q[i];
        *sp = s;
}

/*
 * Fetch file over SLIP on serial port unit.  Returns 0 or an error.
 */
char *
tftpget(unit, file)
int unit;
char *file;
{
        register int n, i;
        int tries;

        if (!sport(unit))
                return (badport);
        quad(&file, srv);
        for (i = 0; i < 3; i++)
                me[i] = srv[i];
        me[3] = srv[3] + 1;
        xseg = (romconf.rom_eram >> 6) - 1 - XSEGS;
        xlen = 0;

        put16(out + 28, 1);             /* RRQ file octet */
        for (n = 30; *file && n < 90; )
                out[n++] = *file++;
        out[n++] = 0;
        for (file = "octet"; *file; )
                out[n++] = *file++;
        out[n++] = 0;
        puts("TFTP ");
        send(69, n - 28);
        tid = 0;
        blk = 1;
        for (tries = 0; tries < 5; ) {
                if ((n = frame()) < 0) {
                        tries++;
                        resend();
                        continue;
                }
                if (n == 0 || tid && w16(pkt + 20) != tid)
                        continue;
                if (w16(pkt + 28) == 5) {
                        puts("refused\n");
                        xseg = 0;
                        return ("server error");
                }
                if (w16(pkt + 28) != 3)
                        continue;
                i = w16(pkt + 30);
                if (i != blk && i != blk - 1)
                        continue;
                if (tid == 0)
                        tid = w16(pkt + 20);
                put16(out + 28, 4);     /* ACK */
                put16(out + 30, i);
                send(tid, 4);
                tries = 0;
                if (i != blk)
                        continue;
                n -= 32;
                xlen += n;
                if ((blk++ & 31) == 0)
                        putchar('.');
                if (n < 512) {
                        puts("\nReceived ");
                        memdiag((unsigned)((xlen + 1023) >> 10));
                        puts("K\n");
                        return (0);
                }
                if (blk > BLKMAX) {
                        xseg = 0;
                        return ("file too big for network boot");
                }
        }
        puts("\n");
        xseg = 0;
        return ("no answer from the server");
}
#endif
