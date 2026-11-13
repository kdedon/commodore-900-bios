/*
 * Commodore M-series boot ROM.
 * Memory test: two complementary patterns through each RAM segment with
 * the block instructions, counting up the KB tested, then a tag per
 * segment to catch address aliasing.  RAM is left clear.
 */

#define WINDOW  7               /* KB of ROM data at the top of RAM */

extern long   mkfp();
extern char   abflags[];
extern        outstr();
extern        memdiag();
extern        puthex();
extern        outw();

unsigned mgot;                  /* word read at a failure */

static unsigned pat[] = { 0x5555, 0xaaaa };

memtest(low, hi)
unsigned low, hi;               /* RAM span, 1 KB clicks */
{
        register unsigned seg, n, top;
        register int i;
        unsigned kb, off;

        top = hi - WINDOW;
        kb = 0;
        for (seg = low >> 6; (seg << 6) < top; seg++) {
                n = top - (seg << 6);
                if (n > 64)
                        n = 64;
                for (i = 0; i < 2; i++) {
                        mfill(seg << 8, pat[i], n << 9);
                        if ((off = mscan(seg << 8, pat[i], n << 9)) != 0) {
                                bad(seg, off - 2, pat[i]);
                                return;
                        }
                }
                mfill(seg << 8, 0, n << 9);
                kb += n;
                putchar('\r');
                outstr("Memory test: ");
                memdiag(kb);
                putchar('K');
        }
        for (seg = low >> 6; (seg << 6) < top; seg++)
                *(unsigned *)mkfp(seg << 8, 0) = seg;
        for (seg = low >> 6; (seg << 6) < top; seg++) {
                if ((mgot = *(unsigned *)mkfp(seg << 8, 0)) != seg) {
                        bad(seg, 0, seg);
                        return;
                }
                *(unsigned *)mkfp(seg << 8, 0) = 0;
        }
        putchar('\r');
        outstr("Memory test: ");
        memdiag(hi - low);
        outstr("K OK\n");
        outw(0x001d, 0x0090);
}

/*
 * Report the failing word as the 1985 ROM did (bank:chip from the lowest
 * bad bit and the segment, also posted on the diagnostic port), with its
 * address and contents, and stop the boot.
 */
bad(seg, off, want)
unsigned seg, off, want;
{
        register unsigned x, bit, code;

        x = want ^ mgot;
        for (bit = 0; x != 0 && (x & 1) == 0; bit++)
                x >>= 1;
        code = bit + (seg << 1);
        outw(0x001d, code);
        outstr("\nRAM ERROR   BANK:CHIP= ");
        puthex((long)code, 4);
        outstr(" at ");
        puthex((long)seg, 4);
        putchar('|');
        puthex((long)off, 12);
        outstr(" wrote ");
        puthex((long)want, 12);
        outstr(" read ");
        puthex((long)mgot, 12);
        putchar('\n');
        abflags[0] = 0xff;
}
