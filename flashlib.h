/* FLASH routines for AM29F016 (16-MBit, 32 Sectors, 64Kbyte sector size),
 * nothing else is supported.
 */
#ifndef __FLASHLIB_H__
#define __FLASHLIB_H__

/* FLASH device description */
struct flashlib_device {
	unsigned int start;		/* Start address of FLASH */
	int sector_count;		/* Number of sectors */
	unsigned int sector_size;	/* Size of one sector */
};

/* FLASH device information read from FLASH device (autoselect) */
struct flashlib_dev_info {
	unsigned char manufacter; 	/* Manufacter ID (AMD=1) */
	unsigned char device;		/* Device ID (AM29F016=AD) */
	char secgrp_protected[32];	/* Sector Group Protection */
};

/* Initializae the library for a certain device described by 'dev' */
extern void *flashlib_init(struct flashlib_device *dev);

/* Reset the current command state of the FLASH */
extern void flashlib_reset(void *handle);

/* Get information about the FLASH, stored into 'info' */
extern int flashlib_info(void *handle, struct flashlib_dev_info *info);

/* Erase the complete FLASH chip content */
extern int flashlib_erase_chip(void *handle);

/* Erase one sector in FLASH, sector_start must an address to the first byte
 * of a sector.
 */
extern int flashlib_erase_sector(void *handle, int sector_start);

/* Erase an address range. Start must be aligned on sector boundary, end
 * does not need to be aligned on sector boundary
 */
extern int flashlib_erase(void *handle, int start, int end);

/* Program FLASH content with data found in 'data', length is given in
 * number of bytes, start is the address in FLASH that the data will be
 * written to
 */
extern int flashlib_program(
	void *handle,
	unsigned int start,
	int length,
	char *data);

#endif
