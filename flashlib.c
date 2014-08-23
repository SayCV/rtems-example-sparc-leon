/* FLASH routines for AM29F016 (16-MBit, 32 Sectors, 64Kbyte sector size) */

#include <stdio.h>
#include <stdlib.h>
#include <rtems.h>

#include "flashlib.h"

/* CPU IRQ Level macros */
#define IRQ_GLOBAL_PREPARE(level) rtems_interrupt_level level
#define IRQ_GLOBAL_DISABLE(level) rtems_interrupt_disable(level)
#define IRQ_GLOBAL_ENABLE(level) rtems_interrupt_enable(level)

/* Load one Byte, Force cache miss */
#define LOAD_BYTE(address) _load_byte(address)

static __inline__ unsigned char _load_byte(volatile unsigned char *adr)
{
	unsigned char tmp;
	asm(" lduba [%1]1, %0 "
		: "=r" (tmp)
		: "r" (adr)
	);
	return tmp;
}

#define FLASH_WAIT() {volatile int i = 100; while ( i > 0 )	i--; }

void *flashlib_init(struct flashlib_device *dev)
{
	return dev;
}

void flashlib_reset(void *handle)
{
	struct flashlib_device *dev = handle;
	volatile unsigned char *start;

	start = (volatile unsigned char *)dev->start;

	/* Start Autoselect command */
	start[0] = 0xF0;

	/* Wait some time */
	FLASH_WAIT();
}

int flashlib_info(void *handle, struct flashlib_dev_info *info)
{
	struct flashlib_device *dev = handle;
	volatile unsigned char *start;
	int i;

	flashlib_reset(handle);

	start = (volatile unsigned char *)dev->start;

	/* Start Autoselect command */
	start[0x5555] = 0xAA;
	start[0x2AAA] = 0x55;
	start[0x5555] = 0x90;

	/* Get FLASH Device Information */
	info->manufacter = LOAD_BYTE(&start[0x0000]);
	info->device = LOAD_BYTE(&start[0x0001]);

	/* Get Sector Group Protection information. Four sectors per group */
	for (i=0; i<dev->sector_count/4; i++) {
		info->secgrp_protected[i] = LOAD_BYTE(&start[dev->sector_size*i*4 + 0x0002]);
	}

	/* Stop auto select command */
	flashlib_reset(handle);

	return 0;
}

/* Erase the complete FLASH chip */
int flashlib_erase_chip(void *handle)
{
	struct flashlib_device *dev = handle;
	volatile unsigned char *start;
	int i;

	printf("CHIP ERASE\n");

	flashlib_reset(handle);

	start = (volatile unsigned char *)dev->start;

	/* Start Chip Erase command */
	start[0x5555] = 0xAA;
	start[0x2AAA] = 0x55;
	start[0x5555] = 0x80;
	start[0x5555] = 0xAA;
	start[0x2AAA] = 0x55;
	start[0x5555] = 0x10;

	/* Wait some time */
	FLASH_WAIT();

	/* Check if erase chip is done */
	i = 0;
	while ( (LOAD_BYTE(&start[0]) & 0x80) == 0 ) {
		printf("Waiting Chip Erase %d\n", i);
		rtems_task_wake_after(50);
		i++;
	}

	/* Stop command */
	flashlib_reset(handle);

	return 0;
}

/* Errase a Specific Sector */
int flashlib_erase_sector(void *handle, int sector_start)
{
	struct flashlib_device *dev = handle;
	volatile unsigned char *start;
	int i;

	printf("SECTOR %d ERASE\n",
		((sector_start - dev->start) / dev->sector_size));

	flashlib_reset(handle);

	start = (volatile unsigned char *)dev->start;

	/* Start Sector Erase command */
	start[0x5555] = 0xAA;
	start[0x2AAA] = 0x55;
	start[0x5555] = 0x80;
	start[0x5555] = 0xAA;
	start[0x2AAA] = 0x55;
	*(volatile unsigned char *)sector_start = 0x30;

	/* Wait some time */
	FLASH_WAIT();

	/* Check if sector erase is done */
	i = 0;
	while ( (LOAD_BYTE((volatile unsigned char *)sector_start) & 0x80) == 0 ) {
		printf("Waiting Sector Erase %d\n", i);
		rtems_task_wake_after(10);
		i++;
	}

	/* Stop command */
	flashlib_reset(handle);

	return 0;
}

/* Erase a range of FLASH */
int flashlib_erase(void *handle, int start, int end)
{
	struct flashlib_device *dev = handle;
	unsigned int sector_mask = dev->sector_size-1;
	int sectcnt, i;

	printf("RANGE ERASE: 0x%08x - 0x%08x\n", start, end);

	if ( start > end )
		return -1;

	if ( start & sector_mask ) {
		/* Start must be aligned to sector size */
		return -2;
	}

	if ( end >= (dev->sector_count*dev->sector_size + dev->start) ) {
		/* End of device */
		return -3;
	}

	/* Calculate end sector */
	end = end & ~sector_mask;

	/* Calculate number of sectors to erase */
	sectcnt = 1 + (end - start) / dev->sector_size;

	if ( (start == dev->start) && (sectcnt == dev->sector_count) ) {
		/* All chip is to be erased, call optimized function */
		return flashlib_erase_chip(handle);
	}

	printf("ERASING %d SECTORS STARTING FROM 0x%08x\n", sectcnt, start);

	/* Erase selected sectors */
	for (i=0; i<sectcnt; i++) {

		/* Erase one Sector */
		if ( flashlib_erase_sector(handle, start) ) {
			return -4;
		}

		start += dev->sector_size;
	}

	return 0;
}

/* Program a data buffer into FLASH memory of device, note that the FLASH must
 * be erased before programmed.
 */
int flashlib_program(void *handle, unsigned int start, int length, char *data)
{
	struct flashlib_device *dev = handle;
	volatile unsigned char *sc, *curr, *end;
	unsigned char byte;

	printf("PROGRAM: 0x%08x - 0x%08x\n", start, start + length);

	if ( dev->start > start ) {
		return -1;
	}
	if ( (dev->start + (dev->sector_size*dev->sector_count)) < (start+length) ) {
		return -2;
	}

	flashlib_reset(handle);

	sc = (volatile unsigned char *)dev->start;
	curr = (volatile unsigned char *)start;
	end = curr + length;

	while ( curr < end ) {

		/* Start Program command */
		sc[0x5555] = 0xAA;
		sc[0x2AAA] = 0x55;
		sc[0x5555] = 0xA0;

		/* Write Byte */
		byte = *data;
		*curr = byte;

		data++;

		/* Check if byte program is done. DQ7 is inverse what is programmed */
		while ( ((LOAD_BYTE(curr) ^ byte) & 0x80) != 0 ) {
			;
		}

		curr++;
	}

	/* Stop command */
	flashlib_reset(handle);

	return 0;
}
