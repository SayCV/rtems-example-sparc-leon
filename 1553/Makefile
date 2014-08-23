# RTEMS SPARC Compiler (4.10)
CC=/opt/rtems-4.10/bin/sparc-rtems-gcc

# Location of Driver-Manager Configuration Files
CFGDIR=..

# Build flags
CFLAGS=-g3 -O0 -Wall
LIBS=

# CPU Dependent compiler config
LEON2= -qleon2
LEON3=

.PHONY:all rtems-gr1553bm rtems-gr1553bcbm rtems-gr1553rtbm linux_client test1
all: rtems-gr1553bm rtems-gr1553bcbm rtems-gr1553rtbm

# BC and BM
rtems-gr1553bcbm:
	$(CC) $(CFLAGS) -c time.c
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON3) -DSOFT_EXTTRIG_ENABLE rtems-gr1553bcbm.c -o rtems-gr1553bcbm time.o $(LIBS)
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON3) rtems-gr1553bcbm.c -o rtems-gr1553bcbm-exttrig time.o $(LIBS)
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON2) -DSOFT_EXTTRIG_ENABLE -DAMBA_OVER_PCI rtems-gr1553bcbm.c -o rtems-gr1553bcbm-leon2 time.o $(LIBS)
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON2) -DAMBA_OVER_PCI rtems-gr1553bcbm.c -o rtems-gr1553bcbm-leon2-exttrig time.o $(LIBS)

# Linux TCP/IP client talking with BM in rtems-gr1553bcbm example
#
# Note that if only builds when the rtems-gr1553bcbm example has been configured
# to support TCP/IP server. See config_bm.h
linux_client:
	gcc -Wall -g3 -O0 linux_client.c -o linux_client

# RT and BM
rtems-gr1553rtbm:
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON3) rtems-gr1553rtbm.c -o rtems-gr1553rtbm $(LIBS)
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON2) -DAMBA_OVER_PCI rtems-gr1553rtbm.c -o rtems-gr1553rtbm-leon2 $(LIBS)

# BM only
rtems-gr1553bm:
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON3) rtems-gr1553bm.c -o rtems-gr1553bm $(LIBS)
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON2) -DAMBA_OVER_PCI rtems-gr1553bm.c -o rtems-gr1553bm-leon2 $(LIBS)

# A AT697+GR-RASTA-IO (BC) TimeMaster distibuting time via SpaceWire to 
# A LEON3 GR-RASTA-105 TimeSlave (BC). The GR1553B BC core distributes time onto a 1553 bus to
# A Standard LEON3 with GR1553B RT core acting as a RT Time Slave
test1:
	# The Two BC Apps
	mkdir -p test1
	$(CC) $(CFLAGS) -c time.c
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON3) -DTIME_SYNC_MANAGEMENT \
		rtems-gr1553bcbm.c -o test1/rtems-gr1553bcbm-test1-leon3 time.o $(LIBS)
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON2) -DAMBA_OVER_PCI -DTIME_SYNC_MANAGEMENT -DTIME_SYNC_MASTER \
		rtems-gr1553bcbm.c -o test1/rtems-gr1553bcbm-test1-leon2 time.o $(LIBS)
	# The RT Application
	$(CC) $(CFLAGS) -I$(CFGDIR) $(LEON3) rtems-gr1553rtbm.c -o test1/rtems-gr1553rtbm-test1-leon3 $(LIBS)


clean:
	rm -rf \
		*.o \
		rtems-gr1553bm \
		rtems-gr1553bm-leon2 \
		rtems-gr1553rtbm \
		rtems-gr1553rtbm-leon2 \
		rtems-gr1553bcbm \
		rtems-gr1553bcbm-exttrig \
		rtems-gr1553bcbm-leon2 \
		rtems-gr1553bcbm-leon2-exttrig \
		linux_client
