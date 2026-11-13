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
extern        mmuread();
extern        mmuwrite();

unsigned mgot;                  /* word read at a failure */
unsigned mwant;                 /* and the word expected */
#if ROMKB > 32
char    segok[32];              /* tested good, one bit per 64 KB segment */
char    segbad[32];             /* failed above the video holes */
#endif

#define SETBIT(m, s)    ((m)[(s) >> 3] |= 1 << ((s) & 7))

static unsigned pat[] = { 0x5555, 0xaaaa };

memtest(low, hi)
unsigned low, hi;               /* RAM span, 1 KB clicks */
{
        register unsigned seg, n, top;
        unsigned kb, off;
        unsigned high;

        top = hi - WINDOW;
        kb = 0;
        for (seg = low >> 6; (seg << 6) < top; seg++) {
                n = top - (seg << 6);
                if (n > 64)
                        n = 64;
                if ((off = mtest(seg, n)) != 0) {
                        bad(seg, off - 2, mwant);
                        return;
                }
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
        high = 0;
#if ROMKB > 32
        for (seg = low >> 6; seg < hi >> 6; seg++)
                SETBIT(segok, seg);
        if ((hi >> 6) == 0x37)
                high = himem(kb);
#endif
        putchar('\r');
        outstr("Memory test: ");
        memdiag(hi - low + high);
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

#if ROMKB > 32
/*
 * RAM card memory above the video holes (segments 0x37, 0x3E, 0x3F),
 * reached through a scratch segment.  A segment that fails is marked
 * bad and left out.  Returns the KB found good.
 */
#define SCRATCH 0x2f

static char     save[4];
static char     desc[4] = { 0, 0, 0xff, 0 };

static unsigned *
look(seg)
unsigned seg;
{
        desc[0] = seg;
        mmuwrite(SCRATCH, desc);
        return ((unsigned *)mkfp(SCRATCH << 8, 0));
}

static
hole(seg)
register unsigned seg;
{
        return (seg < 0x38 || seg == 0x3e || seg == 0x3f || seg > 0xfe);
}

himem(kb)
unsigned kb;
{
        register unsigned seg, *p;
        unsigned good;
        char fit[32];

        mmuread(SCRATCH, 1, save);
        for (seg = 0; seg < 32; seg++)
                fit[seg] = 0;
        for (seg = 0x38; seg <= 0xfe; seg++) {
                if (hole(seg))
                        continue;
                p = look(seg);
                *p = 0x5aa5;
                if (*p != 0x5aa5)
                        continue;
                *p = 0xa55a;
                if (*p == 0xa55a)
                        SETBIT(fit, seg);
        }
        for (seg = 0x38; seg <= 0xfe; seg++)
                if (fit[seg >> 3] & 1 << (seg & 7))
                        *look(seg) = seg;
        good = 0;
        for (seg = 0x38; seg <= 0xfe; seg++) {
                if ((fit[seg >> 3] & 1 << (seg & 7)) == 0)
                        continue;
                if (*look(seg) != seg || mtest(SCRATCH, 64) != 0) {
                        SETBIT(segbad, seg);
                        continue;
                }
                SETBIT(segok, seg);
                good += 64;
                putchar('\r');
                outstr("Memory test: ");
                memdiag(kb + good);
                putchar('K');
        }
        mmuwrite(SCRATCH, save);
        return (good);
}
#endif

/*
 * Both patterns through n KB of logical segment seg, then clear it.
 * Returns 0, or the bad offset + 2 with the pattern in mwant.
 */
mtest(seg, n)
unsigned seg, n;
{
        register int i;
        register unsigned off;

        for (i = 0; i < 2; i++) {
                mfill(seg << 8, pat[i], n << 9);
                if ((off = mscan(seg << 8, pat[i], n << 9)) != 0) {
                        mwant = pat[i];
                        return (off);
                }
        }
        mfill(seg << 8, 0, n << 9);
        return (0);
}
