/* SD Card demo application using the SPICTRL SPI driver.
 *
 * The application tries to mount a FAT filesystem on the first 
 * partition on the SD Card to /mnt in the filesystem. When the
 * set up is completed it traverses filesystem starting a /,
 * when a file matching the name "README.TXT" or "readme.txt"
 * is found anywhere in the filesystem it concatenates the 
 * content of the file to the console.
 *
 * Copyright (C),
 * Aeroflex Gaisler 2009,
 *
 */

#include <rtems.h>
#define CONFIGURE_INIT
#include <bsp.h> /* for device driver prototypes */

rtems_task Init( rtems_task_argument argument);	/* forward declaration needed */

/* configure RTEMS kernel */
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_NULL_DRIVER
#define CONFIGURE_MAXIMUM_TASKS             16
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_EXTRA_TASK_STACKS         (64 * RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 32
#define CONFIGURE_INIT_TASK_PRIORITY	100
#define CONFIGURE_MAXIMUM_DRIVERS 16
#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM

#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK


/* Configure RTEMS kernel */
#include <rtems/confdefs.h>

/* Configure Driver Manager Kernel */
#include <drvmgr/drvmgr.h>
#if defined(RTEMS_DRVMGR_STARTUP) && defined(LEON3) /* if --drvmgr was given to configure */
 /* Add Timer and UART Driver for this example */
 #ifdef CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_GPTIMER
 #endif
 #ifdef CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
  #define CONFIGURE_DRIVER_AMBAPP_GAISLER_APBUART
 #endif
#endif
#define CONFIGURE_DRIVER_AMBAPP_GAISLER_SPICTRL
#include <drvmgr/drvmgr_confdefs.h>

#include <rtems.h>
#include <rtems/libi2c.h>
#include <rtems/libio.h>
#include <spictrl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <rtems/fsmount.h>
#include <rtems/dosfs.h>
#include <rtems/ide_part_table.h>

#undef ENABLE_NETWORK_SMC_LEON3
#undef ENABLE_NETWORK_SMC_LEON2
#undef ENABLE_NETWORK
/* Include driver configurations and system initialization */
#include "config.c"

void directory_listing(char *path, int level);

#include <libchip/spi-sd-card.h>

#define SD_CARD_NUMBER 1

sd_card_driver_entry sd_card_driver_table [SD_CARD_NUMBER] =
{
	{
		.device_name = "/dev/sd-card-a",
		.bus = 1,
		.transfer_mode = SD_CARD_TRANSFER_MODE_DEFAULT,
		.command = SD_CARD_COMMAND_DEFAULT,
		/*.response is used internally */
		.response_index = SD_CARD_COMMAND_SIZE,
		.n_ac_max = SD_CARD_N_AC_MAX_DEFAULT,
		.block_number = 0,
		.block_size = 0,
		.block_size_shift = 0,
		.busy = 1,
		.verbose = 1,
		.schedule_if_busy = 0,
	}
};

size_t sd_card_driver_table_size = SD_CARD_NUMBER;

static fstab_t sdcard_fs_table [] = { {
		"/dev/sd-card-a1", "/mnt",
		&msdos_ops, RTEMS_FILESYSTEM_READ_WRITE,
		FSMOUNT_MNT_OK | FSMOUNT_MNTPNT_CRTERR | FSMOUNT_MNT_FAILED,
		FSMOUNT_MNT_OK
	}, {
		"/dev/sd-card-a", "/mnt",
		&msdos_ops, RTEMS_FILESYSTEM_READ_WRITE,
		FSMOUNT_MNT_OK | FSMOUNT_MNTPNT_CRTERR | FSMOUNT_MNT_FAILED,
		0
	}
};

/* ========================================================= 
   initialisation */

rtems_task Init(
  rtems_task_argument ignored
)
{
	rtems_status_code status;

	printf("******** Initializing SPICTRL SD-CARD test ********\n");

	/* Initialize Driver manager, in config.c */
	system_init();

/*
	rtems_drvmgr_print_devs(0xfffff);
	rtems_drvmgr_print_topo();
*/

	/* The driver has initialized the i2c library for us */

	printf("Registering SPI SD-CARD driver: ");	
	fflush(NULL);
	status = sd_card_register();
	if (status < 0) {
		printf("ERROR: Could not register SPI SD-CARD driver\n");
		exit(0);
	}
	printf("registered successfully\n");

	printf("Initializing IDE partion table: ");
	fflush(NULL);
	status = rtems_ide_part_table_initialize("/dev/sd-card-a");
	if (status != RTEMS_SUCCESSFUL) {
		printf("ERROR: Could not create partion table (%d)\n", status);
		exit(0);
	}
	printf("successfully\n");

	status = mkdir( "/mnt", S_IRWXU);
	if (status != RTEMS_SUCCESSFUL) {
		printf("ERROR: Failed to create /mnt (%d)\n", status);
		exit(0);
	}

	status = rtems_fsmount(sdcard_fs_table, sizeof(sdcard_fs_table)/sizeof(fstab_t), NULL);
	if (status != RTEMS_SUCCESSFUL) {
		printf("ERROR: Mounting SD Card to /mnt Failed (%d)\n", status);
		exit(0);
	}
	
	/* List the directory structure of the filesystem. Look at /mnt
	 * for the SD Card filesystem.
	 *
	 * It will also print the content of all readme.txt it finds in the
	 * filesystem.
	 */
	directory_listing("/", 1);

	exit(0);
}

#include <unistd.h>
#include <sys/dirent.h>
#include <ctype.h>

void directory_listing(char *path, int level)
{
	struct dirent *d;
	DIR *dir;
	int status, i, last;
	char charbuf[128];
	static char filebuf[257];
	static struct stat statbuf;
	
	dir = opendir(path);
	if ( dir == NULL ) {
		printf("Failed to open directory %s\n", path);
		return;
	}

	while( (d = readdir(dir)) != NULL) {
		for ( i=0; i<level*2; i++) {
			charbuf[i] = ' ';
		}
		charbuf[i++] = '|';
		charbuf[i++] = '-';
		charbuf[i++] = '\0';
		if ( strcmp(path, "/") == 0 ) {
			printf("%s %s\n", charbuf, d->d_name);
			sprintf(&charbuf[0], "%s", d->d_name);
		} else {
			printf("%s %s/%s\n", charbuf, path, d->d_name);
			sprintf(&charbuf[0], "%s/%s", path, d->d_name);
		}
		/* Get info if this is a Directory or not */

		status = stat(charbuf, &statbuf);
		if ( status != 0 ) {
			/*
			printf("Failed to stat %s (%d, %d, %s)\n", charbuf, status, errno, strerror(errno));
			*/

			/* Failure, try lower case */
			if ( strcmp(path, "/") == 0 ){
				charbuf[0] = '\0';
				last = 0;
			} else {
				last = sprintf(&charbuf[0], "%s/", path);
			}
			i=0;
			while ( d->d_name[i] != 0 ) {
				charbuf[last++] = tolower(d->d_name[i]);
				i++;
			}
			charbuf[last] = '\0';
			if ( i == 0 )
				return;
			status = stat(charbuf, &statbuf);
			if ( status != 0 ) {
				continue;
			}
		}
		/* If this is a directory recurse down */
		if ( (strcmp(d->d_name, ".") != 0) && (strcmp(d->d_name, "..") != 0) 
			&& (statbuf.st_mode & S_IFDIR) ) {
			/* Statbuf will be destroyed after this */
			directory_listing(charbuf, level+1);
		} else if ( (statbuf.st_mode & S_IFREG) && ((strcmp(d->d_name, "readme.txt") == 0)
			|| (strcmp(d->d_name, "README.TXT") == 0)) ) {
			/* Assuming text file */
			int fd, len;
			fd = open(charbuf, O_RDONLY);
			if ( fd >= 0 ) {
				printf("Contents in %s\n", charbuf);
				while (	(len = read(fd, filebuf, 256)) > 0 ) {
					filebuf[len]='\0';
					printf("%s", filebuf);
				}
				if ( len < 0 ) {
					printf("\n\nError reading file: %s (%d)\n", strerror(errno), errno);
				}
				close(fd);
				printf("\n");
			} else {
				printf("Failed to open %s (%d, %s)\n", charbuf, errno, strerror(errno));
			}
			
		}
	}

	return;

}
