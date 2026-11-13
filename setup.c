/*
 * Commodore M-series boot ROM.
 * Setup menu, entered with C during the power-on tests.  Choices last for
 * this boot only.
 */
#if ROMKB > 32
#include "rom.h"

extern int    conpoll();
extern        tstart();
extern unsigned tnow();
extern        tstop();
extern        memdiag();
extern        puthex();
extern char   abflags[];
extern int    drivetype;
extern struct hdpar {
        char    *hd_type;
        char    hd_param[10];
} hdpar[];

char    wantsetup;              /* C was pressed */
char    bootord;                /* BO_* */
char    skipmem;                /* size memory without testing it */
static char cwait = 10;         /* tenths of a second for C after the tests */

extern long   mkfp();

#define BO_BOTH 0               /* floppy, then hard disk */
#define BO_HD   1
#define BO_FD   2
#define BO_MON  3               /* the monitor, no automatic boot */

static char *order[] = { "floppy, then hard disk", "hard disk", "floppy",
                         "monitor" };

/*
 * Defaults from the block romcfg writes at the end of the image: "C9DF",
 * version 1, memory test, boot order (3: monitor), hard disk type, wait
 * for C in tenths, and a checksum word making the words sum to 0.  A
 * block that does not check out is ignored.
 */
cfgload()
{
        register unsigned char *p;
        register unsigned *w, sum;

        p = (unsigned char *)mkfp(0, 0xfff0);
        if (p[0] != 'C' || p[1] != '9' || p[2] != 'D' || p[3] != 'F'
            || p[4] != 1)
                return;
        sum = 0;
        for (w = (unsigned *)p; w < (unsigned *)(p + 16); w++)
                sum += *w;
        if (sum != 0)
                return;
        skipmem = p[5] != 0;
        if (p[6] < 4)
                bootord = p[6];
        if (p[7] < 4)
                drivetype = p[7];
        if (p[8] != 0)
                cwait = p[8];
}

/*
 * Note a C or Space typed while the tests run.  Returns 1 for Space.
 */
setkey()
{
        register int c;

        c = conpoll();
        if (c == 'c' || c == 'C')
                wantsetup = 1;
        return (c == ' ');
}

/*
 * After the tests: one more second for the C key, then the menu.
 */
setup()
{
        tstart();
        while (!wantsetup && tnow() < cwait * 10)
                setkey();
        tstop();
        while (wantsetup)
                menu();
        if (bootord == BO_MON)
                abflags[1] = 0xff;
}

menu()
{
        register int c;

        for (;;) {
                puts("\nSetup -- for this boot only\n");
                puts("  1  Hardware\n");
                puts("  2  Boot from: ");
                puts(order[bootord]);
                puts("\n  3  Hard disk: ");
                puts(hdpar[drivetype].hd_type);
                puts("\n");
                puts("  C  Continue\n> ");
                c = getchar();
                putchar('\n');
                switch (c) {
                case '1':
                        hardware();
                        break;
                case '2':
                        bootord = (bootord + 1) % 4;
                        break;
                case '3':
                        drivetype = (drivetype + 1) % 4;
                        break;
                case 'c':
                case 'C':
                        wantsetup = 0;
                        return;
                }
        }
}

/*
 * What the ROM found, from the handoff block.
 */
hardware()
{
        register struct romconf *rp;
        register int i;
        unsigned kb;

        rp = &romconf;
        puts("RAM:");
        kb = 0;
        for (i = 0; i < rp->rc_nram; i++) {
                putchar(' ');
                puthex((long)(rp->rc_ram[i].seg & 0xff), 4);
                putchar('-');
                puthex((long)((rp->rc_ram[i].seg & 0xff)
                    + (rp->rc_ram[i].nseg & 0xff) - 1), 4);
                kb += (rp->rc_ram[i].nseg & 0xff) * 64;
        }
        puts(", ");
        memdiag(kb);
        puts("K\nVideo:");
        if (rp->rc_video == 0)
                puts(" none");
        if (rp->rc_video & RV_HR)
                puts(rp->rc_hr404 & 1 ? " hi-res colour" : " hi-res mono");
        if (rp->rc_video & RV_LR)
                puts(" lo-res");
        if (rp->rc_video & RV_LC)
                puts(" 8563");
        puts("\nConsole: ");
        puts(rp->rc_console == RC_CON_HR ? "hi-res" :
             rp->rc_console == RC_CON_LR ? "lo-res" : "serial");
        putchar('\n');
}
#endif
