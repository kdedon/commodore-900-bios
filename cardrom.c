/*
 * Commodore M-series boot ROM.
 * The LANCE card's own boot ROM, read through its window into the
 * reserved segment, mapped and started.  It fetches the file (le) names
 * and hands it over as disk unit 4, block by block.
 */
#if ROMKB > 32
#include "rom.h"

#define ROMA    0x0718          /* window address, words */
#define ROMD    0x071a          /* window data, address + 1 per read */

struct ndev {
        char    nd_magic[4];    /* "C9ND" */
        unsigned nd_version;
        char    nd_name[2];
        int     nd_uoff;
        char    *(*nd_open)();
        int     (*nd_read)();
        char    *(*nd_info)();
};

extern int    inw();
extern        outw();
extern long   mkfp();
extern        segload();
extern        mmuwrite();
extern char   romabi[];

char    cardoff;                /* setup: leave the card ROM alone */
static struct ndev *ndp;

/* Put logical segments s to s+2 back on themselves. */
static
unmap(s)
unsigned s;
{
        static char desc[4] = { 0, 0, 0xff, 0 };
        register int i;

        for (i = 0; i < 3; i++) {
                desc[0] = s + i;
                mmuwrite(s + i, desc);
        }
}

/*
 * Header: "C9NR", version 1, length in words, entry offset, text
 * segment, flags 0, then a checksum making the image's words sum to 0.
 */
cardrom()
{
        register unsigned *p, sum;
        register unsigned i;
        unsigned h[8], page;
        struct ndev *(*init)();
        char *r;

        if (cardoff || (romconf.rc_net & RN_LANCE) == 0)
                return;
        outw(ROMA, 0);
        for (i = 0; i < 8; i++)
                h[i] = inw(ROMD);
        if (h[0] != 0x4339 || h[1] != 0x4e52)
                return;
        r = "bad header";
        if (h[2] != 1 || h[3] < 9 || h[3] > 0x2000 || h[4] >= 2 * h[3]
            || h[6] != 0 || h[5] < 2 || h[5] > 0x34)
                goto fail;
        page = (romconf.rc_rsvd_seg & 0xff) << 8 | 0x40;
        p = (unsigned *)mkfp(page & 0xff00, 0x4000);
        outw(ROMA, 0);
        sum = 0;
        for (i = 0; i < h[3]; i++)
                sum += *p++ = inw(ROMD);
        r = "bad checksum";
        if (sum != 0)
                goto fail;
        segload(h[5], page, page);
        init = (struct ndev *(*)())mkfp(h[5] << 8, h[4]);
        ndp = (*init)(&romconf, romabi, page);
        if (ndp == 0 || ndp->nd_magic[0] != 'C' || ndp->nd_magic[1] != '9'
            || ndp->nd_magic[2] != 'N' || ndp->nd_magic[3] != 'D'
            || ndp->nd_version != 1 || ndp->nd_name[0] != 'n'
            || ndp->nd_name[1] != 'd') {
                ndp = 0;
                unmap(h[5]);
                r = "no device";
                goto fail;
        }
        ndinfo();
        return;
fail:
        puts("Network card ROM: ");
        puts(r);
        putchar('\n');
}

/* Start fetching arg, "[server:]file".  0 or an error. */
char *
ndopen(arg)
char *arg;
{
        if (ndp == 0)
                return ("no network card ROM");
        return ((*ndp->nd_open)(arg));
}

/* The file's blocks, in order: 1 ok, 0 past its end */
ndread(unit, blk, buf)
int unit;
long blk;
char *buf;
{
        if (ndp == 0)
                return (0);
        return ((*ndp->nd_read)(unit, blk, buf));
}

ndinfo()
{
        if (ndp == 0) {
                puts("No network card ROM\n");
                return;
        }
        puts("Network: ");
        puts((*ndp->nd_info)());
        putchar('\n');
}
#endif
