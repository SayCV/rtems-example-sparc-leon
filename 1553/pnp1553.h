/* Example of simple PnP header. Nothing thought through... */

/* 32 byte PnP header */
struct pnp1553info {
	uint16_t vendor;		/* 0x00 */
	uint16_t device;		/* 0x02 */

	uint16_t version;		/* 0x04 */
	uint16_t class;			/* 0x06 */

	uint16_t subadr_rx_avail;	/* 0x08 */
	uint16_t subadr_tx_avail;	/* 0x0A */

	char     desc[20];		/* 0x0C */
};
