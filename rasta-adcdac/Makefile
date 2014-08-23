#
# Configuration Options
#

# Define LEON to -qleon2 for LEON2 systems
#LEON=-qleon2

# Define LEON2_GRLIB for GRLIB LEON2 Plug & Play systems
#LEON2_GRLIB=-DLEON2_GRLIB=1

# OUT controlls the name of the resultant binary
OUT=rtems-rasta-adcdac

INCLUDE=-I..
CC=/opt/rtems-4.10/bin/sparc-rtems-gcc

CFLAGS=-O0 -g $(LEON2_GRLIB) $(LEON)

all:
	make clean
	make OUT=rtems-rasta-adcdac-leon3 rtems-rasta-adcdac
	make clean
	make LEON=-qleon2 LEON2_GRLIB="-DLEON2_GRLIB=1" OUT=rtems-rasta-adcdac-leon2grlib rtems-rasta-adcdac
	make clean
	make LEON=-qleon2 OUT=rtems-rasta-adcdac-leon2 rtems-rasta-adcdac

rtems-rasta-adcdac: rtems-rasta-adcdac.c gradcdac-demo.c gpio-demo.c
	$(CC) $(CFLAGS) $(INCLUDE) -c gradcdac-demo.c -o gradcdac-demo.o
	$(CC) $(CFLAGS) $(INCLUDE) -c gpio-demo.c -o gpio-demo.o
	$(CC) $(CFLAGS) $(INCLUDE) rtems-rasta-adcdac.c -o $(OUT) gradcdac-demo.o gpio-demo.o

clean:
	rm -f *.o
