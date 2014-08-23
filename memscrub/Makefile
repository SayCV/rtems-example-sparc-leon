XCFLAGS=-Wall -g -O

all: scrubtest scrubtest_ram0

clean:
	rm -f *.o scrubtest scrubtest_ram0

memscrub.o: memscrub.c memscrub.h
	sparc-rtems-gcc -c memscrub.c $(XCFLAGS)	

memscrub_rtems.o: memscrub_rtems.c memscrub_rtems.h memscrub.h
	sparc-rtems-gcc -c memscrub_rtems.c $(XCFLAGS)

scrubtest.o: scrubtest.c memscrub_rtems.h
	sparc-rtems-gcc -c scrubtest.c -DMEMSTART=0x40000000 $(XCFLAGS)

scrubtest_ram0.o: scrubtest.c memscrub_rtems.h
	sparc-rtems-gcc -c scrubtest.c -o scrubtest_ram0.o -DMEMSTART=0 $(XCFLAGS)

scrubtest: scrubtest.o memscrub_rtems.o memscrub.o
	sparc-rtems-gcc scrubtest.o memscrub_rtems.o memscrub.o -o scrubtest

scrubtest_ram0: scrubtest_ram0.o memscrub_rtems.o memscrub.o
	sparc-rtems-gcc -qngmp scrubtest_ram0.o memscrub_rtems.o memscrub.o -o scrubtest_ram0
