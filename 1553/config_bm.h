
/* An example how to use the custom log copy function to compress
 * data from the BM buffer.
 */
/*#define COMPRESSED_LOGGING*/

#ifdef COMPRESSED_LOGGING
	/* Make BM log available to PC Client over TCP/IP server.
	 *
	 * The ethernet service do only support the compressed log
	 * format.
	 */
	#define ETH_SERVER

	/* Define this if the BC/BM initialization should wait for 
	 * a client to connect.
	 */
	#define BM_WAIT_CLIENT

	/* Port number of TCP/IP connection */
	#define ETHSRV_PORT 20334
#endif
