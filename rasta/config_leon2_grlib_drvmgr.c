#include <drvmgr/leon2_amba_bus.h>
#include <drvmgr/ambapp_bus.h>

struct rtems_drvmgr_key leon2_custom_ambapp[] =
{
	{"REG0", KEY_TYPE_INT, {0xfff00000}}, /* IOAREA of AMBA PnP Bus */
	{"IRQ0", KEY_TYPE_INT, {0}},
	{"IRQ1", KEY_TYPE_INT, {1}},
	{"IRQ2", KEY_TYPE_INT, {2}},
	{"IRQ3", KEY_TYPE_INT, {3}},
	{"IRQ4", KEY_TYPE_INT, {4}},
	{"IRQ5", KEY_TYPE_INT, {5}},
	{"IRQ6", KEY_TYPE_INT, {6}},
	{"IRQ7", KEY_TYPE_INT, {7}},
	{"IRQ8", KEY_TYPE_INT, {8}},
	{"IRQ9", KEY_TYPE_INT, {9}},
	{"IRQ10", KEY_TYPE_INT, {10}},
	{"IRQ11", KEY_TYPE_INT, {11}},
	{"IRQ12", KEY_TYPE_INT, {12}},
	{"IRQ13", KEY_TYPE_INT, {13}},
	{"IRQ14", KEY_TYPE_INT, {14}},
	{"IRQ15", KEY_TYPE_INT, {15}},
	KEY_EMPTY
};

struct leon2_core leon2_amba_custom_cores[] =
{
	/* GRLIB-LEON2 has a AMBA Plug & Play bus */
	{{LEON2_AMBA_AMBAPP_ID}, "AMBAPP", &leon2_custom_ambapp[0]},
	EMPTY_LEON2_CORE
};

struct leon2_bus leon2_bus_config =
{
	&drv_mgr_leon2_std_cores[0],		/* The standard cores */
	&leon2_amba_custom_cores[0],		/* custom cores */
	NULL
};

/* AMBA Resources
 */
struct rtems_drvmgr_drv_res ambapp_drv_resources[] =
{
	RES_EMPTY
};

/* AMBAPP resources */
struct rtems_drvmgr_key leon2_amba_ambapp0_res[] =
{
/* {"freq", KEY_TYPE_INTEGER, {(unsigned int)40000000}}, force 40MHz on AMBA-PnP bus */
	{"drvRes", KEY_TYPE_POINTER, {(unsigned int)&ambapp_drv_resources}},
	KEY_EMPTY
};

/* Driver resources on LEON2 AMBA bus */
struct rtems_drvmgr_drv_res leon2_amba_res[] =
{
	{DRIVER_LEON2_AMBA_AMBAPP, 0, &leon2_amba_ambapp0_res[0]},
	RES_EMPTY
};

/* GRLIB-LEON2 specific system init */
void system_init2(void)
{
#ifndef RTEMS_DRVMGR_STARTUP
	/* LEON2 PnP bus on top of standard LEON2 Bus 
	 * Note that this is only for GRLIB-LEON2 systems.
	 *
	 * Register LEON2 root bus
	 */
	drv_mgr_leon2_init(&leon2_bus_config, &leon2_amba_res[0]);
#endif
}
