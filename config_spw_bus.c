#include <drvmgr/drvmgr.h>
#include <drvmgr/spw_bus.h>

/* Include support for GR-RASTA-TMTC over SpaceWire */
#define GR_RASTA_TMTC_RMAP

/* Include support for GR-RASTA-IO over SpaceWire */
/*#define GR_RASTA_IO_RMAP */

#ifdef GR_RASTA_TMTC_RMAP
/* External Memory configuration */
struct rtems_drvmgr_key ambapp_rmap_grlib1_mctrl0_res[] =
{
	{"mcfg1", KEY_TYPE_INT, {(unsigned int) 0x000002ff}},
	{"mcfg2", KEY_TYPE_INT, {(unsigned int) 0x00001260}}, /* 0x00001265 */
	{"mcfg3", KEY_TYPE_INT, {(unsigned int) 0x000e8000}},

	/* Memory partitions available on the AMBA RMAP bus for drivers (GRTM and GRTC). Buffers are allocated
	 * using "ambapp_rmap_partition_memalign(PART_NUM, LENGTH)" from the drivers. So it is important that the
	 * partitions are number according to what the drivers expect: See GRTM/GRTC configuration below
	 *
	 * PART0 = 2MBytes SRAM
	 */
	{"part0Start",  KEY_TYPE_INT, {(unsigned int) 0x40000000}}, /* Use 2Mb SRAM to ambapp_rmap_partition_memalign() */
	{"part0Length", KEY_TYPE_INT, {(unsigned int) 0x00200000}},

	/* WASH RAM settings */
	{"wash1Start",  KEY_TYPE_INT, {(unsigned int) 0x40000000}}, /* 4Mb SRAM (wash is not limited to partition size) */
	{"wash1Length", KEY_TYPE_INT, {(unsigned int) 0x00400000}}, /* Wash the SRAM memory (after EDAC is enabled) */
};

/* GRTM-RMAP - GRTM configuration
 *
 * GRTM will allocate BD_CNT * (BD_SIZE + FRAME_LENGTH) from the two different partitions, where
 *   BD_CNT:         Number of descriptors (normally 128)
 *   BD_SIZE:        Size of one Descriptor = 8 (2 32-bit words)
 *   FRAME_LENGTH:   Max Length of Frame aligned
 *
 * In this configuration: ~141 Kbytes.
 */
struct rtems_drvmgr_key ambapp_rmap_grlib1_grtm0_res[] =
{
	/* MAX Frame Length, used to allocate buffers. */
	{"maxFrameLength", KEY_TYPE_INT, {(unsigned int) 1120}}, /* 1115 Framelength */

	/* Allocate BD descriptor table from Memory Partition 1 (SRAM: see MCTRL config) */
	{"bdAllocPartition", KEY_TYPE_INT, {(unsigned int) 0}},

	/* Allocate Frame Buffers from Memory Partition 1 (SRAM: see MCTRL config) */
	{"frameAllocPartition", KEY_TYPE_INT, {(unsigned int) 0}}, 
	KEY_EMPTY
};

/* All Driver resources on the GR-TMTC02 AMBA bus */
struct rtems_drvmgr_drv_res ambapp_rmap_grlib0_res[] =
{
	{DRIVER_AMBAPP_ESA_MCTRL_ID, 0, &ambapp_rmap_grlib1_mctrl0_res[0]},
	{DRIVER_AMBAPP_GAISLER_GRTM_ID, 0, &ambapp_rmap_grlib1_grtm0_res[0]},
};

/* GR-RASTA-TMTC: GRLIB AMBA Plug&Play over SpW bus Driver Configuration, for device unit 0 */
struct rtems_drvmgr_key spw_bus0_grlib1_res[] = 
{
	{"IOArea", KEY_TYPE_INT, {(unsigned int)0x80200000}},
	{"BusFreq", KEY_TYPE_INT, {(unsigned int)33000000}}, /* Clocked taken from PCI clock */
	{"BusRes", KEY_TYPE_POINTER,{(unsigned int)&ambapp_rmap_grlib1_res[0]}},
	KEY_EMPTY
};

/* GR-TMTC0 SpW Node hardware set up. */
struct rtems_drvmgr_key gr_tmtc_spw_target[] =
{
	{"DST_ADR", KEY_TYPE_INT, 0xFE},
	{"DST_KEY", KEY_TYPE_INT, 0},
	{"VIRQ1", KEY_TYPE_INT, SPWBUS_VIRQ1}, /* Virtual IRQ index=1 of this SpW-Node, is connected to Virtual IRQ 1 */
	KEY_EMPTY
};
#endif

#ifdef GR_RASTA_IO_RMAP
/* GR-RASTA-IO: GRLIB AMBA Plug&Play over SpW bus Driver Configuration, for device unit 0 */
struct rtems_drvmgr_key spw_bus0_grlib2_res[] = 
{
	{"IOArea", KEY_TYPE_INT, {(unsigned int)0x80100000}},
	{"BusFreq", KEY_TYPE_INT, {(unsigned int)50000000}},
	KEY_EMPTY
};

/* GR-IO0 SpW Node hardware set up. */
struct rtems_drvmgr_key gr_io_spw_target[] =
{
	{"DST_ADR", KEY_TYPE_INT, 0xFE},
	{"DST_KEY", KEY_TYPE_INT, 0},
	{"VIRQ1", KEY_TYPE_INT, SPWBUS_VIRQ1}, /* Virtual IRQ index=1 of this SpW-Node, is connected to Virtual IRQ 1 */
	KEY_EMPTY
};
#endif

/* Driver Resources on SpW bus, each entry in array represents one SpW node identified
 * by SpW Driver ID and unit number
 */
struct rtems_drvmgr_drv_res spw_bus0_resources[] =
{
#ifdef GR_RASTA_TMTC_RMAP
	/* GR-RASTA-TMTC */	{DRIVER_SPW_RMAP_AMBAPP_ID, 0, &spw_bus0_grlib1_res[0]},
#endif
#ifdef GR_RASTA_IO_RMAP
	/* GR-RASTA-IO */	{DRIVER_SPW_RMAP_AMBAPP_ID, 0, &spw_bus0_grlib2_res[0]},
#endif
	RES_EMPTY
};


/* SpaceWire nodes on the network that should be available to the
 * Driver Manager. Note that the order of SpW nodes will affect the
 * unit number.
 */
struct spw_node spw0_nodes[] =
{
#ifdef GR_RASTA_TMTC_RMAP
	{{SPW_NODE_ID_GRLIB}, "GR-RASTA-TMTC0", &gr_tmtc_spw_target[0]},
#endif
#ifdef GR_RASTA_IO_RMAP
	{{SPW_NODE_ID_GRLIB}, "GR-RASTA-IO0", &gr_io_spw_target[0]},
#endif
	EMPTY_SPW_NODE
};

/* SpaceWire bus, bus driver configuration paramters */
struct spw_bus_config bus_cfg =
{
	.rmap			= NULL,				/* Set later */
	.maxlen			= 0x800,			/* 0x800 Max length of RMAP packets */
	.nodes			= &spw0_nodes[0],		/* SpW Nodes on network */
	.devName		= {0,0},			/* Set Later */
	.resources		= &spw_bus0_resources[0],	/* Driver resources for all SpW Nodes */
	.virq_table		=
	{	/* VIRTUAL IRQ (SpW Bus IRQ) to GPIO Pin IRQ table */
		/* VIRQ1 */ {"/dev/grgpio0/1", NULL},  /* Use the GPIO1 PIN to get IRQ from (system IRQ1) */
		/* VIRQ2 */ {NULL, NULL},
		/* VIRQ3 */ {NULL, NULL},
		/* VIRQ4 */ {NULL, NULL},
	},
};

/* This will register the SpW bus to the Driver Manager, the driver manager will scan through
 * the SpW bus and init the drivers for the SpW nodes, and the buses discovered by the SpW Nodes.
 *
 * Note that this function is typically called after the Local AMBA bus is up running to ensure
 * that the local system is OK and SpW link is OK before proceeding.
 *
 * Arguments
 *   devname          - Device Name of "driver manager device", this device is used to attach the
 *                      SpW bus to, this device will be the bus device. It will not know about it,
 *                      however the driver manager will understand the topology, so that if SPW
 *                      device is remove this SpW bus will also be removed.
 *   rmap_stack_priv  - Private data used by the RMAP stack to communicate with the SpW bus.
 *
 */
int system_init_rmap_bus(char *devname, void *rmap_stack_priv)
{
	bus_cfg.rmap = rmap_stack_priv;
	strcpy(bus_cfg.devName, devname);

	if ( spw_bus_register(&bus_cfg) ) {
		printf("Failed to init SpW Bus\n");
		return -1;
	}

	return 0;
}
