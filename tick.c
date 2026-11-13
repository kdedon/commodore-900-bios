/*
 * Commodore M-series boot ROM.
 * Waits in hundredths of a second, counted by CIO 1 counter/timer 2 from
 * the fixed 6 MHz clock, so they last as long at any CPU speed.  Counter 3
 * is left to programs that keep time with it.
 */
#if ROMKB > 32
#define MCCR    0x0003          /* master configuration control */
#define CT2CS   0x0017          /* command and status */
#define CT2CH   0x0031          /* time constant */
#define CT2CL   0x0033
#define CT2MS   0x003b          /* mode */

#define CT2E    0x20            /* MCCR: counter 2 enabled */
#define IP      0x20            /* status: terminal count reached */

extern int    inb();
extern        outb();

static unsigned ticks;

/*
 * Count from 0 at 100 Hz: half the 6 MHz clock divided by 30000.
 */
tstart()
{
        outb(MCCR, inb(MCCR) & ~CT2E);
        outb(CT2MS, 0x80);              /* continuous */
        outb(CT2CH, 30000 >> 8);
        outb(CT2CL, 30000 & 0xff);
        outb(CT2CS, IP);                /* clear a stale pending bit */
        outb(MCCR, inb(MCCR) | CT2E);
        outb(CT2CS, 0x06);              /* gate, trigger */
        ticks = 0;
}

/*
 * Hundredths since tstart; call at least every 10 ms.
 */
unsigned
tnow()
{
        if (inb(CT2CS) & IP) {
                outb(CT2CS, 0xa4);      /* clear IP, keep the gate open */
                ticks++;
        }
        return (ticks);
}

tstop()
{
        outb(CT2CS, 0);
        outb(MCCR, inb(MCCR) & ~CT2E);
}

/*
 * Wait n hundredths.
 */
twait(n)
register unsigned n;
{
        tstart();
        while (tnow() < n)
                ;
        tstop();
}
#endif
