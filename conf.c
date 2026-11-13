/*
 * Commodore M-series boot ROM.
 * Establish machine configuration parameters.
 */
#include "rom.h"
#include "ddtvars.h"

#define LCADDR	0x0181		/* 8563 address register / status */
#define LCDATA	0x0183
#define LCID	0x01ff		/* reads 0xc1 */

extern int    inw();
extern        outw();
extern int    inb();
extern        outb();
extern char   segok[], segbad[];

configure(rp, ramlow, ramhigh)
register struct romconf *rp;
unsigned ramlow, ramhigh;
{
	register char *p;

	rp->rom_bram = ramlow;
	rp->rom_eram = ramhigh;
	rp->rom_ctype = CTYPE;
#if ROMKB > 32
	for (p = rp->rc_magic; p < (char *)(rp + 1); p++)
		*p = 0;
	rp->rc_magic[0] = 'C';
	rp->rc_magic[1] = '9';
	rp->rc_magic[2] = 'R';
	rp->rc_magic[3] = 'C';
	rp->rc_version = 1;
	rp->rc_length = sizeof *rp;
	rp->rc_romver = 0x0200;
	if (rp->has_hires) {
		rp->rc_video |= RV_HR;
		rp->rc_hr404 = inw(0x0404);
		rp->rc_console = RC_CON_HR;
	} else if (ddtvars.d_conlr) {
		rp->rc_video |= RV_LR;
		rp->rc_console = RC_CON_LR;
	} else
		rp->rc_console = RC_CON_SER;
	if (lanprobe(rp->rc_mac))
		rp->rc_net |= RN_LANCE;
	if (lcprobe()) {
		rp->rc_video |= RV_LC;
		rp->rc_lcid = 0xc1;
		rp->rc_lcver = inb(LCADDR) & 3;
	}
	rp->rc_serial = 0x0001;
	runs(rp, ramlow >> 6, ramhigh >> 6);
	rp->rc_rsvd_seg = (ramhigh >> 6) - 1;	/* data window at the top */
	rp->rc_rsvd_nseg = 1;
	rcseal(rp);
#endif
}

#if ROMKB > 32
/*
 * Write v to 8563 register r unless v is negative, then return what r
 * reads, or -1 if the chip never says ready.
 */
static
lcreg(r, v)
int r, v;
{
	register int i;

	for (;;) {
		outb(LCADDR, r);
		for (i = 1000; (inb(LCADDR) & 0x80) == 0; )
			if (--i == 0)
				return (-1);
		if (v < 0)
			return (inb(LCDATA) & 0xff);
		outb(LCDATA, v);
		v = -1;
	}
}

/*
 * The 8563 card: its ID byte, read after writing other values so a bus
 * that echoes or floats cannot pass, then the update address R18/R19
 * holding what is written.  The registers are put back.
 */
lcprobe()
{
	register int i, ok;
	int r18, r19;

	for (i = 0; i < 2; i++) {
		outb(LCID, i ? 0x94 : 0x3e);
		if ((inb(LCID) & 0xff) != 0xc1)
			return (0);
	}
	r18 = lcreg(18, -1);
	r19 = lcreg(19, -1);
	ok = lcreg(18, 0x5a) == 0x5a && lcreg(19, 0xa5) == 0xa5
	  && lcreg(18, 0xa5) == 0xa5 && lcreg(19, 0x5a) == 0x5a;
	lcreg(18, r18 & 0xff);
	lcreg(19, r19 & 0xff);
	return (ok);
}

/*
 * The Am7990 card: CSR0 through RAP 0 reads with STOP set after reset,
 * and 0xffff with no card.  Its address PROM goes into mac.
 */
lanprobe(mac)
register unsigned *mac;
{
	register unsigned csr0;
	register int i;

	outw(0x0702, 0);
	csr0 = inw(0x0700);
	if (csr0 == 0xffff || (csr0 & 0x0004) == 0)
		return (0);
	for (i = 0; i < 3; i++)
		mac[i] = inw(0x0710 + 2 * i);
	return (1);
}

/*
 * RAM runs from the segments the memory test passed, or the sized span
 * [lo, hi) when it did not run.
 */
runs(rp, lo, hi)
register struct romconf *rp;
unsigned lo, hi;
{
	register unsigned seg, n;
	register int any;

	any = 0;
	for (seg = 0; seg < 32; seg++) {
		any |= segok[seg];
		rp->rc_badseg[seg] = segbad[seg];
	}
	n = 0;
	for (seg = 0; seg < 0x100 && n < 16; seg++) {
		if (any ? (segok[seg >> 3] & 1 << (seg & 7)) == 0
			: seg < lo || seg >= hi)
			continue;
		if (n > 0 && (rp->rc_ram[n-1].seg & 0xff)
		    + (rp->rc_ram[n-1].nseg & 0xff) == seg
		    && (rp->rc_ram[n-1].nseg & 0xff) < 0xff)
			rp->rc_ram[n-1].nseg++;
		else {
			rp->rc_ram[n].seg = seg;
			rp->rc_ram[n].nseg = 1;
			n++;
		}
	}
	rp->rc_nram = n;
}

/*
 * Record the boot device and drive geometry, and set the checksum.
 */
rcboot(rp, dev, unit, off, cyl, heads, secs)
register struct romconf *rp;
char *dev;
int unit, cyl, heads, secs;
long off;
{
	rp->rc_bootdev[0] = dev[0];
	rp->rc_bootdev[1] = dev[1];
	rp->rc_bootunit = unit;
	rp->rc_bootoff = off;
	rp->rc_hdcyl = cyl;
	rp->rc_hdheads = heads;
	rp->rc_hdsec = secs;
	rcseal(rp);
}

rcseal(rp)
register struct romconf *rp;
{
	register unsigned *w, sum;

	rp->rc_cksum = 0;
	sum = 0;
	for (w = (unsigned *)rp->rc_magic; w < (unsigned *)(rp + 1); w++)
		sum += *w;
	rp->rc_cksum = -sum;
}
#endif
