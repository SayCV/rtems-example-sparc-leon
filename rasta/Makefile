CC=/opt/rtems-4.10/bin/sparc-rtems-gcc
LD=/opt/rtems-4.10/bin/sparc-rtems-ld
OBJDUMP=/opt/rtems-4.10/bin/sparc-rtems-objdump

PROGS = rtems-rasta-demo rtems-rasta-demo-can rtems-rasta-demo-spw rtems-rasta-demo-brm \
        \
        rtems-canloopback \
        rtems-brm_bc rtems-brm_bm rtems-brm_rt \
        
#rtems-rasta-demo-uart

COPTS = -O2 
CFLAGS = -Wall -g3 $(COPTS)

L2=-qleon2
L3=

CFGDEPS = config.c config_leon3_drvmgr.c config_leon2_drvmgr.c config_leon2_grlib_drvmgr.c

all: $(PROGS)
	

# Simple help functions for RASTA demo and the boards used to communicate
# with the RASTA demo.
#
vt100.o: vt100.c vt100.h
	$(CC) $(L3) $(CFLAGS) -c vt100.c -o vt100.o

occan_lib.o: occan_lib.c occan_lib.h
	$(CC) $(L3) $(CFLAGS) -c occan_lib.c -o occan_lib.o

brm_lib_leon2.o: brm_lib.c brm_lib.h
	$(CC) $(L2) -g -c brm_lib.c -o brm_lib_leon2.o

brm_lib_leon3.o: brm_lib.c brm_lib.h
	$(CC) $(L3) -g -c brm_lib.c -o brm_lib_leon3.o

# For a LEON3 OC_CAN target used to test the CAN (GRHCAN) interface for the RASTA demo
#        
rtems-canloopback: vt100.o occan_lib.o rtems-canloopback.c
	$(CC) $(L3) $(CFLAGS) rtems-canloopback.c -o rtems-canloopback vt100.o occan_lib.o
	$(OBJDUMP) -S rtems-canloopback > rtems-canloopback.S

# For a LEON3 BRM target used to test the BRM interface for the RASTA demo
#
rtems-brm_bc: rtems-brm.c brm_lib_leon3.o
	$(CC) $(L3) -Wall -g $(CCOPT) -DBRM_BC_TEST rtems-brm.c brm_lib_leon3.o -o rtems-brm_bc

rtems-brm_rt: rtems-brm.c brm_lib_leon3.o
	$(CC) $(L3) -Wall -g $(CCOPT) rtems-brm.c brm_lib_leon3.o -o rtems-brm_rt

rtems-brm_bm: rtems-brm.c brm_lib_leon3.o
	$(CC) $(L3) -Wall -g $(CCOPT) -DBRM_BM_TEST rtems-brm.c brm_lib_leon3.o -o rtems-brm_bm

#
# The main LEON 2 RASTA DEMO
#
#
rtems-rasta-demo: $(CFGDEPS) rtems-rasta-demo.c apbuart-demo.o grspw-demo.o grcan-demo.o b1553-demo.o
	$(CC) $(L2) $(CFLAGS) -DSPW_TEST rtems-rasta-demo.c -o rtems-rasta-demo apbuart-demo.o grspw-demo.o grcan-demo.o b1553-demo.o brm_lib_leon2.o
	$(OBJDUMP) -S rtems-rasta-demo > rtems-rasta-demo.S

apbuart-demo.o: apbuart-demo.c
	$(CC) -c -g $(L2) $(CFLAGS) apbuart-demo.c -o apbuart-demo.o

grspw-demo.o: grspw-demo.c
	$(CC) -c -g $(L2) $(CFLAGS) grspw-demo.c -o grspw-demo.o

grcan-demo.o: grcan-demo.c
	$(CC) -c -g $(L2) $(CFLAGS) grcan-demo.c -o grcan-demo.o

b1553-demo.o: b1553-demo.c brm_lib_leon2.o
	$(CC) -c -g $(L2) $(CFLAGS) b1553-demo.c -o b1553-demo.o

#
# Simpler RASTA demos, using a single interface
#

# RASTA UART demo
rtems-rasta-demo-uart: $(CFGDEPS) apbuart-demo.o rtems-rasta-demo.c
	$(CC) $(L2) $(CFLAGS) -DUART_TEST rtems-rasta-demo.c -o rtems-rasta-demo-uart apbuart-demo.o

# RASTA CAN demo
rtems-rasta-demo-can: $(CFGDEPS) grcan-demo.o rtems-rasta-demo.c
	$(CC) $(L2) $(CFLAGS) -DCAN_TEST rtems-rasta-demo.c -o rtems-rasta-demo-can grcan-demo.o

# RASTA SpaceWire demo
rtems-rasta-demo-spw: $(CFGDEPS) grspw-demo.o rtems-rasta-demo.c
	$(CC) $(L2) $(CFLAGS) -DSPW_TEST rtems-rasta-demo.c -o rtems-rasta-demo-spw grspw-demo.o

# RASTA BRM demo
rtems-rasta-demo-brm: $(CFGDEPS) brm_lib_leon2.o b1553-demo.o rtems-rasta-demo.c
	$(CC) $(L2) $(CFLAGS) -DBRM_TEST rtems-rasta-demo.c -o rtems-rasta-demo-brm b1553-demo.o brm_lib_leon2.o


#
# Clean up mess created when building
#
clean:
	rm -f $(PROGS) *.o *.S
        
