/*
 * Flatten the linked boot ROM (l.out) to the EPROM image: text at 0,
 * data at ROMDATA, zero fill to ROMSIZE.  The header is parsed byte by
 * byte (shorts low-first, longs high word first).
 */

#define ROMSIZE ((long)ROMKB * 1024)
#define CHUNK   4096
#define DATAAT  ((long)(unsigned)ROMDATA)
#if ROMKB > 32
#define CFGSIZE 16                      /* defaults block at the very end */
#else
#define CFGSIZE 0
#endif

char    buf[CHUNK];
char    hdr[44];
int     in, out;

unsigned gw(p)
register char *p;
{
        return ((p[1] & 0xff) << 8) | (p[0] & 0xff);
}

long gl(p)
register char *p;
{
        return ((long)gw(p) << 16) | (long)gw(p + 2);
}

fail(s)
char *s;
{
        register int n;

        for (n = 0; s[n] != '\0'; n++)
                ;
        write(2, s, n);
        exit(1);
}

/* Copy n bytes from in to out, or zeros if zero is set. */
copy(n, zero)
long n;
int zero;
{
        register int i, c;

        if (zero)
                for (i = 0; i < CHUNK; i++)
                        buf[i] = 0;
        while (n > 0) {
                c = n > CHUNK ? CHUNK : n;
                if (!zero && read(in, buf, c) != c)
                        fail("bootfix: short read\n");
                if (write(out, buf, c) != c)
                        fail("bootfix: write failed\n");
                n -= c;
        }
}

main(argc, argv)
int argc;
char *argv[];
{
        register int i;
        int tbase;
        long ssize[9];
        long text, data;
        long lseek();

        if (argc != 3)
                fail("usage: bootfix l.out image\n");
        if ((in = open(argv[1], 0)) < 0)
                fail("bootfix: cannot open\n");
        if (read(in, hdr, 44) != 44)
                fail("bootfix: short header\n");
        tbase = gw(hdr + 6);
        for (i = 0; i < 9; i++)
                ssize[i] = gl(hdr + 8 + 4 * i);
        text = ssize[0];                        /* SHRI */
        data = ssize[3] + ssize[4];             /* SHRD + PRVD */
        if (text > DATAAT || data > ROMSIZE - CFGSIZE - DATAAT)
                fail("bootfix: image too big\n");
        if (data + ssize[5] > WINDOW * 1024L)
                fail("bootfix: data and bss exceed WINDOW\n");
        if ((out = creat(argv[2], 0644)) < 0)
                fail("bootfix: cannot create\n");
        lseek(in, (long)tbase, 0);
        copy(text, 0);
        copy(DATAAT - text, 1);
        lseek(in, (long)tbase + ssize[1] + text, 0);
        copy(data, 0);
        copy(ROMSIZE - DATAAT - data, 1);
        close(in);
        close(out);
        exit(0);
}
