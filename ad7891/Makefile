CC=sparc-rtems-gcc

CFLAGS= -O0 -g

# Uncoment this to make the example run in normal (non-periodic) mode.
#CFLAGS+= -DSINGLE_READ


# Create binaries for the first 4 SPI cores, most systems only
# have one SPI core, thus _{2,3,4} will fail.
spi: spi-ad78xx.c spi-ad78xx.h rtems-spi-ad7891.c
	$(CC) -c $(CFLAGS) spi-ad78xx.c -o spi-ad78xx.o
	$(CC) -DCHAN_NUM=1 -I.. $(CFLAGS) -DSINGLE_READ rtems-spi-ad7891.c -o rtems-spi-ad78xx_1 spi-ad78xx.o
	$(CC) -DCHAN_NUM=2 -I.. $(CFLAGS) rtems-spi-ad7891.c -o rtems-spi-ad78xx_2 spi-ad78xx.o
	$(CC) -DCHAN_NUM=3 -I.. $(CFLAGS) rtems-spi-ad7891.c -o rtems-spi-ad78xx_3 spi-ad78xx.o
	$(CC) -DCHAN_NUM=4 -I.. $(CFLAGS) rtems-spi-ad7891.c -o rtems-spi-ad78xx_4 spi-ad78xx.o

clean:
	rm -f rtems-spi-ad78xx_* *.o
