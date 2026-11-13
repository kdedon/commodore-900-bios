# Makefile for the C900 boot ROM.
#
# This is the NATIVE makefile: run on a booted C900, its defaults are that
# machine's own /bin/cc, /bin/as, /bin/nld and /lib/libc.a, and `make rom' is
# all there is to it.  A host build overrides the six macros below to route
# every tool through the emulator's process runner -- see mk/host.mk, which is
# the entry point for that, and mk/mwc1985/ for the wrappers.  The flags, the
# object list and the order are the original build's and are not macros:
# changing one of those changes the ROM.

CCBIN=cc
ASBIN=as
LDBIN=/bin/nld
CPPBIN=/lib/cpp
LIB=/lib/libc.a
RUN=

# Data sits at ROMDATA in the image.  Reset copies it up to ldrbuf, which
# must stay the last initialised object (porttest.o links last).
# A 32 KB part: make ROMKB=32 ROMDATA=0x6cf0
ROMKB=64
ROMDATA=0xe000
LAYOUT=-DROMKB=$(ROMKB) -DROMDATA=$(ROMDATA)

OPTIONS=-DZ8000HR -DFIVEINCH=1 -DDDT -DAUTOBOOT
CONFIG=-DRAMBASE=0x08000000 -DCMDBLKPADDR="(0x8000000L)"\
  -DBUFPADDR="(0x8000400L)"
CC=$(CCBIN) -c $(OPTIONS) $(CONFIG) $(LAYOUT) -I/usr/sys/z8001/h
LD=$(LDBIN) -i -R 0x00000000 -o rom.out
OBJECTS=crt.o rom.o ramtest.o sio.o hd.o wd.o boot.o diag.o trap.o conf.o\
  ddt.o hires.o io.o loadseg.o lceddt.o mbittest.o diagdisp.o memtest.o\
  tick.o setup.o porttest.o

# bootfix flattens the linked l.out to the 32K ROM image in place;
# `prom rom` then splits it into rom.H (even bytes) + rom.L (odd bytes) for
# the EPROM programmer.  Both are Z8001 programs this build compiles, so on a
# host they need the runner too -- which is what $(RUN) is.
rom: $(OBJECTS) font.o bootfix
	$(LD) $(OBJECTS) $(LIB) font.o
	$(RUN) ./bootfix rom.out rom

split: rom prom
	$(RUN) ./prom rom

bootfix: bootfix.c Makefile
	$(CCBIN) -o bootfix $(LAYOUT) -I/usr/sys/z8001/h bootfix.c
prom: prom.c Makefile
	$(CCBIN) -o prom $(LAYOUT) -I/usr/sys/z8001/h prom.c

font.o: font.s
	$(ASBIN) -go font.o font.s

rom.o boot.o conf.o sio.o hd.o wd.o lceddt.o porttest.o memtest.o: romconf.h ddtvars.h rom.h Makefile

rom.o: rom.c
	$(CC) rom.c
trap.o: trap.c
	$(CC) trap.c
hd.o: hd.c
	$(CC) hd.c
wd.o: wd.c
	$(CC) wd.c
diag.o: diag.c
	$(CC) diag.c
boot.o: boot.c
	$(CC) boot.c
sio.o: sio.c
	$(CC) sio.c
conf.o: conf.c
	$(CC) conf.c

crt.o: lkcrt.s Makefile
	$(CPPBIN) -E -DDDT -DRAMBASE=0x08000000 $(LAYOUT) lkcrt.s >crt.i
	$(ASBIN) -go crt.o crt.i
	rm -f crt.i
ddt.o: ddt.s
	$(CPPBIN) -E -DDDT -DRAMBASE=0x08000000 ddt.s >ddt.i
	$(ASBIN) -go ddt.o ddt.i
	rm -f ddt.i
io.o: io.s
	$(ASBIN) -go io.o io.s
ramtest.o: ramtest.s
	$(ASBIN) -go ramtest.o ramtest.s
mbittest.o: mbittest.s
	$(ASBIN) -go mbittest.o mbittest.s
loadseg.o: loadseg.s
	$(CPPBIN) -E -DNLD -DDDT loadseg.s >loadseg.i
	$(ASBIN) -gox loadseg.o loadseg.i
	rm -f loadseg.i
hires.o: hires.s
	$(CPPBIN) -E -DNLD -DDDT hires.s >hires.i
	$(ASBIN) -gox hires.o hires.i
	rm -f hires.i
lceddt.o: lceddt.c
	$(CCBIN) -c -DK1 -DNLD -DDDT -DFIVEINCH -I/usr/sys/z8001/h -I /usr/sys/h lceddt.c
diagdisp.o: diagdisp.s
	$(CPPBIN) -E -DNLD -DDDT diagdisp.s >diagdisp.i
	$(ASBIN) -gox diagdisp.o diagdisp.i
	rm -f diagdisp.i
memtest.o: memtest.c Makefile
	$(CC) memtest.c
tick.o: tick.c Makefile
	$(CC) tick.c
setup.o: setup.c romconf.h rom.h Makefile
	$(CC) setup.c
porttest.o: porttest.c cioports.h
	$(CC) porttest.c

clean:
	rm -f *.o *.i rom rom.out rom.H rom.L bootfix prom
