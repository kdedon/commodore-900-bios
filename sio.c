/*
 * Commodore M-series boot ROM.
 * Console I/O primitives.  con_hr/con_lr route output to the
 * hi-res bitmap or the lo-res 6845 screen; otherwise the Z8030 SCC.
 */

#include "romconf.h"
#include "ddtvars.h"
#define con_hr     romconf.has_hires          /* hi-res bitmap console */
#define con_lr     ddtvars.d_conlr            /* lo-res 6845 console */
int  get_init = 0;              /* getchar initialized */
char put_init = 0;              /* putchar initialized */

extern        altput();
extern        hrput();
extern        coninit();
extern int    splhi();
extern        splx();
extern        chipinit();
extern int    hrkey();
extern int    hrdecode();
extern int    inb();
extern        outb();

putchar(c)
register char c;
{
        register int s;

        if (con_hr) { altput(c); return; }
        if (con_lr) { hrput(c); return; }
        if (put_init == 0)
                coninit();
        if (c == '\n')
                putchar('\r');
        s = splhi();
        while ((inb(0x0101) & 0x04) == 0)       /* wait for transmitter */
                ;
        outb(0x0111, c);
        splx(s);
}

#if ROMKB > 32
/*
 * A character from the console, or 0 if none is waiting.  No echo.
 */
conpoll()
{
        register int c;

        if (con_hr || con_lr) {
                if (get_init == 0) {
                        chipinit();
                        get_init++;
                }
                if ((c = hrkey()) == 0)
                        return (0);
                return (hrdecode(c));
        }
        if ((inb(0x0101) & 0x01) == 0)
                return (0);
        return (inb(0x0111) & 0x7f);
}
#endif

/*
 * Wait until the serial console has sent everything, so a chip test or
 * reset that follows cannot cut a character short.
 */
drain()
{
        if (con_hr || con_lr)
                return;
        do
                outb(0x0101, 1);
        while ((inb(0x0101) & 0x01) == 0);      /* RR1 All Sent */
}

int
getchar()
{
        register int c;

        if (con_hr || con_lr) {
                if (get_init == 0) {
                        chipinit();
                        get_init++;
                }
                do {
                        while ((c = hrkey()) == 0)
                                ;
                        c = hrdecode(c);
                } while (c == 0);
        } else {
                while ((inb(0x0101) & 0x01) == 0)       /* wait for receiver */
                        ;
                c = inb(0x0111) & 0x7f;                 /* strip parity */
        }
        if (c == '\r')
                c = '\n';
        putchar(c);                                     /* echo */
        return c;
}
