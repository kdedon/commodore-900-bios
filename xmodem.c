/*
 * Commodore M-series boot ROM.
 * Serial boot: (sx,n) receives a load module by XMODEM on serial port n
 * into RAM below the ROM's own segment, then the disk loader runs it.
 */
#if ROMKB > 32
#include "rom.h"

#define SOH     0x01
#define STX     0x02
#define EOT     0x04
#define ACK     0x06
#define NAK     0x15
#define CAN     0x18

#define XSEGS   3               /* 192 KB of receive buffer */

extern int    inb();
extern        outb();
extern long   mkfp();
extern        tstart();
extern unsigned tnow();
extern        tstop();
extern        ldirb();

unsigned xseg;                  /* receive buffer segment, 0: disk boot */
long    xlen;                   /* bytes received */

/* SCC ports in the order of the boot info's serial map */
static unsigned sccbase[] = { 0x0100, 0x0120, 0x0300, 0x0320, 0x0380, 0x03a0 };
static unsigned ctl, dat;
char    badport[] = "bad serial port";

/* 9600 8N1 from the 6 MHz clock: register, value */
static char sccinit[] = {
        4, 0x44, 3, 0xc0, 5, 0x60, 11, 0x50, 12, 17, 13, 0,
        14, 0x02, 14, 0x03, 3, 0xc1, 5, 0xea,
};

/*
 * Use serial port unit, programming it unless it is the console's.
 * Returns 0 if there is no such port.
 */
sport(unit)
register int unit;
{
        register int i;

        if (unit < 0 || unit > 5)
                return (0);
        ctl = sccbase[unit] + 1;
        dat = sccbase[unit] + 0x11;
        if (unit != 0)
                for (i = 0; i < sizeof sccinit; i += 2) {
                        outb(ctl, sccinit[i]);
                        outb(ctl, sccinit[i + 1]);
                }
        return (1);
}

/*
 * A byte from the port, or -1 after t hundredths.
 */
rx(t)
unsigned t;
{
        tstart();
        while ((inb(ctl) & 0x01) == 0)
                if (tnow() >= t) {
                        tstop();
                        return (-1);
                }
        tstop();
        return (inb(dat) & 0xff);
}

tx(c)
int c;
{
        while ((inb(ctl) & 0x04) == 0)
                ;
        outb(dat, c);
}

/* Read until the line is quiet for a second. */
static
purge()
{
        while (rx(100) >= 0)
                ;
}

static unsigned
crc16(crc, c)
register unsigned crc;
int c;
{
        register int i;

        crc ^= c << 8;
        for (i = 0; i < 8; i++)
                crc = crc & 0x8000 ? crc << 1 ^ 0x1021 : crc << 1;
        return (crc);
}

/*
 * Receive into segments seg..seg+XSEGS-1.  Returns the length, or -1.
 */
long
xrecv(seg)
unsigned seg;
{
        register int c, i, n;
        register unsigned sum;
        int crc, tries, b1, b2;
        unsigned blk, want;
        long pos;

        crc = 1;
        for (tries = 0; ; tries++) {
                if (tries == 20)
                        return (-1L);
                if (tries == 10)
                        crc = 0;
                tx(crc ? 'C' : NAK);
                if ((c = rx(300)) == SOH || c == STX)
                        break;
                if (c == CAN)
                        return (-1L);
        }
        blk = 1;
        pos = 0;
        tries = 0;
        for (;;) {
                if (c == EOT) {
                        tx(ACK);
                        return (pos);
                }
                if (c == CAN || tries++ == 10)
                        return (-1L);
                if (c != SOH && c != STX)
                        goto bad;
                n = c == SOH ? 128 : 1024;
                if (pos + n > (long)XSEGS << 16) {
                        tx(CAN);
                        tx(CAN);
                        return (-1L);
                }
                b1 = rx(100);
                b2 = rx(100);
                sum = 0;
                for (i = 0; i < n; i++) {
                        if ((c = rx(100)) < 0)
                                goto bad;
                        *(char *)mkfp((seg + (unsigned)((pos + i) >> 16)) << 8,
                            (unsigned)(pos + i)) = c;
                        sum = crc ? crc16(sum, c) : sum + c;
                }
                if (crc) {
                        want = rx(100) << 8;
                        want |= rx(100) & 0xff;
                } else {
                        want = rx(100);
                        sum &= 0xff;
                }
                if (want != sum || b1 != (~b2 & 0xff))
                        goto bad;
                if (b1 == (blk & 0xff)) {
                        pos += n;
                        blk++;
                } else if (b1 != ((blk - 1) & 0xff)) {
                        tx(CAN);
                        tx(CAN);
                        return (-1L);
                }
                tries = 0;
                tx(ACK);
                c = rx(1000);
                continue;
bad:
                purge();
                tx(NAK);
                c = rx(1000);
        }
}

/*
 * Receive a file on serial port unit for the loader.  Returns 0 or an
 * error.
 */
char *
xget(unit)
int unit;
{
        if (!sport(unit))
                return (badport);
        puts("Waiting for XMODEM\n");
        xseg = (romconf.rom_eram >> 6) - 1 - XSEGS;
        if ((xlen = xrecv(xseg)) < 0) {
                xseg = 0;
                return ("transfer failed");
        }
        puts("Received ");
        memdiag((unsigned)((xlen + 1023) >> 10));
        puts("K\n");
        return (0);
}

/*
 * The next 512 bytes of the received file into buf, zeros past its end.
 */
xread(lbn, buf)
unsigned lbn;
char *buf;
{
        register int i;

        if ((long)lbn << 9 >= xlen) {
                for (i = 0; i < 512; i++)
                        buf[i] = 0;
                return (1);
        }
        ldirb(mkfp((xseg + (lbn >> 7)) << 8, (lbn & 127) << 9), buf, 512);
        return (1);
}
#endif
