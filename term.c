/*
 * Commodore M-series boot ROM.
 * Terminal: the console talks to another machine on a serial port.
 * Ctrl-] then q quits, e toggles local echo, b steps the baud rate, and a
 * second Ctrl-] sends one.
 */
#if ROMKB > 32
#include "rom.h"
#include "ddtvars.h"

#define ESCKEY  0x1d            /* Ctrl-] */
#define RING    1024

extern int    sport();
extern int    rxpoll();
extern        tx();
extern        setrts();
extern        setbaud();
extern int    conpoll();
extern char   badport[];

static char     ring[RING];
static int      head, tail;
static char     tcs[] = { 17, 8, 3 };
static char    *rates[] = { "9600", "18750", "37500" };

/*
 * Run the terminal on serial port unit until q.
 */
term(unit)
int unit;
{
        register int c, n;
        int echo, rate, held;

        if (unit == 0 && !romconf.has_hires && !ddtvars.d_conlr) {
                puts("port 0 is the console\n");
                return;
        }
        if (!sport(unit)) {
                puts(badport);
                putchar('\n');
                return;
        }
        puts("Terminal on port ");
        putchar('0' + unit);
        puts(", 9600 baud.  Ctrl-] q quits.\n");
        echo = rate = held = 0;
        head = tail = 0;
        for (;;) {
                while ((c = rxpoll()) >= 0) {
                        ring[head] = c;
                        head = (head + 1) % RING;
                        if (head == tail)       /* full: drop the oldest */
                                tail = (tail + 1) % RING;
                }
                n = (head - tail + RING) % RING;
                if (!held && n > RING * 3 / 4) {
                        setrts(0);
                        held = 1;
                } else if (held && n < RING / 4) {
                        setrts(1);
                        held = 0;
                }
                if (n) {
                        c = ring[tail] & 0x7f;
                        tail = (tail + 1) % RING;
                        if (c != 0)
                                putchar(c);
                }
                if ((c = conpoll()) == 0)
                        continue;
                if (c == ESCKEY) {
                        while ((c = conpoll()) == 0)
                                ;
                        if (c == 'q' || c == 'Q')
                                break;
                        if (c == 'e' || c == 'E') {
                                echo = !echo;
                                puts(echo ? "\n[echo on]\n" : "\n[echo off]\n");
                                continue;
                        }
                        if (c == 'b' || c == 'B') {
                                rate = (rate + 1) % 3;
                                setbaud(tcs[rate]);
                                puts("\n[");
                                puts(rates[rate]);
                                puts(" baud]\n");
                                continue;
                        }
                        if (c != ESCKEY)
                                continue;
                }
                tx(c);
                if (echo)
                        putchar(c == '\r' ? '\n' : c);
        }
        setrts(1);
        puts("\n");
}
#endif
