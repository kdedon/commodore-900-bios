# Makefile for the C900 boot ROM.

OPTIONS=-DZ8000HR -DFIVEINCH=1 -DDDT -DAUTOBOOT
CONFIG=-DRAMBASE=0x08000000 -DCMDBLKPADDR="(0x8000000L)"\
  -DBUFPADDR="(0x8000400L)"
CC=cc -c $(OPTIONS) $(CONFIG) -I/usr/sys/z8001/h
LD=/bin/nld -i -R 0x00000000 -o rom
LIB=/lib/libc.a
OBJECTS=crt.o rom.o ramtest.o sio.o hd.o wd.o boot.o diag.o trap.o conf.o\
  ddt.o hires.o io.o loadseg.o lceddt.o mbittest.o diagdisp.o porttest.o

# bootfix flattens the linked l.out to the 32K ROM image in place;
# `prom rom` then splits it into rom.H/rom.L for the EPROM programmer.
rom: $(OBJECTS) font.o bootfix
	$(LD) $(OBJECTS) $(LIB) font.o
	bootfix rom

bootfix: bootfix.c
	cc -o bootfix -I/usr/sys/z8001/h bootfix.c
prom: prom.c
	cc -o prom -I/usr/sys/z8001/h prom.c

font.o: font.s
	as -go font.o font.s

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

crt.o: lkcrt.s
	/lib/cpp -E -DDDT -DRAMBASE=0x08000000 lkcrt.s >crt.i
	as -go crt.o crt.i
	rm -f crt.i
ddt.o: ddt.s
	/lib/cpp -E -DDDT -DRAMBASE=0x08000000 ddt.s >ddt.i
	as -go ddt.o ddt.i
	rm -f ddt.i
io.o: io.s
	as -go io.o io.s
ramtest.o: ramtest.s
	as -go ramtest.o ramtest.s
mbittest.o: mbittest.s
	as -go mbittest.o mbittest.s
loadseg.o: loadseg.s
	/lib/cpp -E -DNLD -DDDT loadseg.s >loadseg.i
	as -gox loadseg.o loadseg.i
	rm -f loadseg.i
hires.o: hires.s
	/lib/cpp -E -DNLD -DDDT hires.s >hires.i
	as -gox hires.o hires.i
	rm -f hires.i
lceddt.o: lceddt.c
	cc -c -DK1 -DNLD -DDDT -DFIVEINCH -I/usr/sys/z8001/h -I /usr/sys/h lceddt.c
diagdisp.o: diagdisp.s
	/lib/cpp -E -DNLD -DDDT diagdisp.s >diagdisp.i
	as -gox diagdisp.o diagdisp.i
	rm -f diagdisp.i
porttest.o: porttest.c cioports.h
	$(CC) porttest.c
