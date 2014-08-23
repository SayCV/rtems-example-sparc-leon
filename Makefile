CC=sparc-rtems-gcc
CPP=sparc-rtems-c++
CC_NOCFLAGS=sparc-rtems-gcc
LD=sparc-rtems-ld
MKPROM2=mkprom2

# ERC32 applications
ERC32_PROG = rtems-hello rtems-cdtest \
	rtems-tasks rtems-synctrap

# LEON2 applications
LEON2_PROG = rtems-ttcp rtems-hello rtems-cdtest \
	rtems-tasks rtems-synctrap rtems-http \
	rtems-ttcpw rtems-ttcp

# LEON3 applications
LEON3_PROGS=rtems-brm_bm rtems-brm_rt rtems-brm_bc \
            rtems-occan rtems-occan_tx rtems-occan_rx \
            rtems-spwtest_2boards_rx rtems-spwtest_2boards_tx \
            rtems-spwtest_loopback rtems-i2cmst \
	    rtems-grcan rtems-grcan_rx rtems-grcan_tx \
	    rtems-pci rtems-b1553rt rtems-spi rtems-spi-sdcard \
	    rtems-gpio
# rtems-watchdog

LEON3MP_PROGS=rtems-mp

CCOPT = -O2
CFLAGS=
OUTDIR=

all: leon3v8 leon3fpv8 leon3 leon3fp leon2v8 leon2fpv8 leon2 leon2fp

build: $(LEON2_PROG)

build_leon3: $(LEON3_PROGS)

build_erc32: $(ERC32_PROG)

leon2:
	$(MAKE) OUTDIR="bin/leon2/v7/" CFLAGS="-msoft-float -qleon2" build

leon2fp:
	$(MAKE) OUTDIR="bin/leon2/v7fp/" CFLAGS="-qleon2" build

leon2fpv8:
	$(MAKE) OUTDIR="bin/leon2/v8fp/" CFLAGS="-qleon2 -mcpu=v8" build

leon2v8:
	$(MAKE) OUTDIR="bin/leon2/v8/" CFLAGS="-qleon2 -mcpu=v8 -msoft-float" build

leon3:
	$(MAKE) OUTDIR="bin/leon3/v7/" CFLAGS="-msoft-float -g" build build_leon3

leon3fp:
	$(MAKE) OUTDIR="bin/leon3/v7fp/" CFLAGS="" build  build_leon3

leon3fpv8:
	$(MAKE) OUTDIR="bin/leon3/v8fp/" CFLAGS="-mcpu=v8" build build_leon3

leon3v8:
	$(MAKE) OUTDIR="bin/leon3/v8/" CFLAGS="-mcpu=v8 -msoft-float" build build_leon3

erc32:
	$(MAKE) OUTDIR="bin/erc32/" CFLAGS=-tsc691 build_erc32

rtems-hello: rtems-hello.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-hello.c -o $(OUTDIR)rtems-hello

rtems-hello-multiple: rtems-hello-multiple.c
	$(CC) -g3 -O0 $(CFLAGS) $(CCOPT) rtems-hello-multiple.c -o $(OUTDIR)rtems-hello-multiple

# assume for debugging
rtems-uart-loopback: rtems-uart-loopback.c
	$(CC) -g3 -O0 $(CFLAGS) $(CCOPT) rtems-uart-loopback.c -o $(OUTDIR)rtems-uart-loopback

rtems-tasks: rtems-tasks.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-tasks.c -o $(OUTDIR)rtems-tasks

rtems-irq: rtems-irq.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-irq.c -o $(OUTDIR)rtems-irq

rtems-synctrap: rtems-synctrap.c
	$(CC) -g $(CFLAGS) rtems-synctrap.c -o $(OUTDIR)rtems-synctrap

rtems-cdtest: rtems-cdtest.cc
	$(CPP) -g $(CFLAGS) $(CCOPT) rtems-cdtest.cc -o $(OUTDIR)rtems-cdtest

rtems-http: rtems-http.c networkconfig.h rootfs/etc/hosts rootfs/etc/host.conf \
	rootfs/index.html
	cd rootfs ; tar cf ../tarfile web etc index.html
	$(CC) -g -c $(CFLAGS) $(CCOPT) rtems-http.c -o $(OUTDIR)rtems-http.o
	$(LD) -g -r -o $(OUTDIR)temp.o $(OUTDIR)rtems-http.o -b binary tarfile
	$(CC) -g $(CFLAGS) $(CCOPT) $(OUTDIR)temp.o -o $(OUTDIR)rtems-http

rtems-ttcp: rtems-ttcp.c
	$(CC) -g $(CFLAGS) $(CCOPT) -msoft-float -DREAD_TEST_ONLY rtems-ttcp.c -o $(OUTDIR)rtems-ttcp

rtems-io: rtems-io.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-io.c -o $(OUTDIR)rtems-io

rtems-ttcpw: rtems-ttcp.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DWRITE_TEST_ONLY rtems-ttcp.c -o $(OUTDIR)rtems-ttcpw

rtems-pd: rtems-pd.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DREAD_TEST_ONLY rtems-pd.c -o $(OUTDIR)rtems-pd

# only LEON3
rtems-occan: rtems-occan.c occan_lib.h occan_lib.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DTASK_TX -DTASK_RX rtems-occan.c occan_lib.c -o $(OUTDIR)rtems-occan

rtems-occan_tx: rtems-occan.c occan_lib.h occan_lib.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DMULTI_BOARD -DTASK_TX rtems-occan.c occan_lib.c -o $(OUTDIR)rtems-occan_tx
	
rtems-occan_rx: rtems-occan.c occan_lib.h occan_lib.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DMULTI_BOARD -DTASK_RX rtems-occan.c occan_lib.c -o $(OUTDIR)rtems-occan_rx

rtems-spwtest_2boards_rx: rtems-spwtest-2boards.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DTASK_RX rtems-spwtest-2boards.c -o $(OUTDIR)rtems-spwtest_2boards_rx
	
rtems-spwtest_2boards_tx: rtems-spwtest-2boards.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DTASK_TX rtems-spwtest-2boards.c -o $(OUTDIR)rtems-spwtest_2boards_tx

rtems-spwtest_loopback: rtems-spwtest-2boards.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DTASK_TX -DTASK_RX rtems-spwtest-2boards.c -o $(OUTDIR)rtems-spwtest_loopback

rtems-brm_bc: rtems-brm.c $(OUTDIR)brm_lib.o
	$(CC) -Wall -g $(CFLAGS) $(CCOPT) -DBRM_BC_TEST rtems-brm.c $(OUTDIR)brm_lib.o -o $(OUTDIR)rtems-brm_bc

rtems-brm_rt: rtems-brm.c $(OUTDIR)brm_lib.o
	$(CC) -Wall -g $(CFLAGS) $(CCOPT) rtems-brm.c $(OUTDIR)brm_lib.o -o $(OUTDIR)rtems-brm_rt

rtems-brm_bm: rtems-brm.c $(OUTDIR)brm_lib.o
	$(CC) -Wall -g $(CFLAGS) $(CCOPT) -DBRM_BM_TEST rtems-brm.c $(OUTDIR)brm_lib.o -o $(OUTDIR)rtems-brm_bm

$(OUTDIR)brm_lib.o: brm_lib.c brm_lib.h
	$(CC) -g $(CFLAGS) -c brm_lib.c -o $(OUTDIR)brm_lib.o

rtems-i2cmst: rtems-i2cmst.c 
	$(CC) -Wall -g $(CFLAGS) $(CCOPT) rtems-i2cmst.c -o $(OUTDIR)rtems-i2cmst

rtems-spi: rtems-spi.c
	$(CC) -Wall -g -O0 $(CFLAGS) $(CCOPT) rtems-spi.c -o $(OUTDIR)rtems-spi

rtems-spi-sdcard: rtems-spi-sdcard.c
	$(CC) -Wall -g -O0 $(CFLAGS) $(CCOPT) rtems-spi-sdcard.c -o $(OUTDIR)rtems-spi-sdcard

# Used to receive messages from rtems-grcan_tx running on another board
rtems-grcan_rx: rtems-grcan.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DCANRX_ONLY rtems-grcan.c -o $(OUTDIR)rtems-grcan_rx

# Used to transmit messages to rtems-grcan_rx running on another board
rtems-grcan_tx: rtems-grcan.c
	$(CC) -g $(CFLAGS) $(CCOPT) -DCANTX_ONLY rtems-grcan.c -o $(OUTDIR)rtems-grcan_tx

# This test assumes an external board is responding to the transmitted 
# messages. similar to rtems-canloopback.
rtems-grcan: rtems-grcan.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-grcan.c -o $(OUTDIR)rtems-grcan

# Sets up PCI configuration space and prints out AMBA & PCI device found
rtems-pci: rtems-pci.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-pci.c -o $(OUTDIR)rtems-pci

# Sets up PCI configuration space and prints out AMBA & PCI device found
rtems-gpio: rtems-gpio.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-gpio.c -o $(OUTDIR)rtems-gpio

# GRPWM 4 channel example application
rtems-grpwm: rtems-grpwm.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-grpwm.c -o $(OUTDIR)rtems-grpwm

rtems-b1553rt: rtems-b1553rt.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-b1553rt.c -o $(OUTDIR)rtems-b1553rt

rtems-watchdog: rtems-watchdog.c
	$(CC) -g $(CFLAGS) $(CCOPT) rtems-watchdog.c -o $(OUTDIR)rtems-watchdog

# This must match the stack and entry point setup in rtems-mp-batch-*.grmon
MP_MAX_NODES=2
SHM_START=0x40000000  # Shared memory is 0x40000000-0x40001000
SHM_SIZE=0x1000
MP_TEXT_1=0x40001000  # 4Mb-1k
MP_TEXT_2=0x40400000  # 4Mb
# Compile RAM images and create FLASH MKPROM image (The MKPROM parameters must be changed)
rtems-mp: rtems-mp.c
	@-nodes=""; \
	 for (( i=1 ; i <= $(MP_MAX_NODES) ; i++ )) ; \
	 do \
	   if [ $$i -eq 1 ]; then \
	     addr=$(MP_TEXT_1); \
	   else \
	     addr=$(MP_TEXT_2); \
	   fi; \
	   nodes=`echo $$nodes rtems-mp$$i`; \
	   echo " Compiling MP node $$i";  \
	   $(CC) -qleon3mp -g $(CFLAGS) $(CCOPT) -DSHM_START=$(SHM_START) -DSHM_SIZE=$(SHM_SIZE) \
	     -DNODE_NUMBER=$$i -Wl,-Ttext,$$addr \
	     rtems-mp.c -o $(OUTDIR)rtems-mp$$i; \
	 done; \
	 echo NODES: $$nodes; \
	 $(MKPROM2) -mp -mpstack 2 0x403fff00 0x407fff00 -mpentry 2 0x40001000 0x40400000 -mpuart 2 0x80000100 0x80000600 \
	                    -baud 38400 -freq 50 -memcfg1 0x0003c2ff -memcfg2 0x92c46000 -memcfg3 0x001d2000 \
			    -nopnp -o rtems-mp.mkprom \
			    $$nodes;
	@echo "rtems-mp-batch-ram.grmon can be used to start the applications from RAM"

clean:
	rm -rf bin/erc32/* bin/*/*/* b_*.c $(LEON2_PROG) $(LEON3_PROGS) *.out *.o tarfile *.srec *.exe *.ali *.o core > /dev/null

rtems-flash: rtems-flash.c
	$(CC) -O0 -g3 -Wall rtems-flash.c -o rtems-flash
