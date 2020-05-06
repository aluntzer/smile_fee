/**
 * this is a very simple FEE <-> DPU interface demonstrator
 *
 * Note that we currently emulate the RMAP connection to the FEE by function
 * calls. The simulated FEE is very stupid, it does not really do anything...
 *
 * Implement your own rmap_tx and rmap_rx interface functions as needed, these
 * can be networked, use actual spacewire or whatever.
 *
 * WARNING: at this point, the FEE houskeeping read-out is not yet implemented
 *	    in the library
 */

#include <stddef.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <smile_fee_cfg.h>
#include <smile_fee_ctrl.h>
#include <smile_fee_rmap.h>


#include <rmap.h>	/* for FEE simulation */

/* whatever for now ... */
#define MAX_PAYLOAD_SIZE	4096

/* XXX include extra for RMAP headers, 128 bytes is plenty */
#undef GRSPW2_DEFAULT_MTU
#define GRSPW2_DEFAULT_MTU (MAX_PAYLOAD_SIZE + 128)



/* for our simulated fee, you can use the accessor part of the library and
 * the stuff below to to control you own FEE simulator
 */
static struct smile_fee_mirror smile_fee_mem;

static uint8_t *rmap_sim_reply[64]; /* same number of buffers as trans log size */
static int rmap_sim_reply_size[64]; /* buffer sizes */
/**
 * @brief create a complete package from header and payload data including CRC8
 *
 * @note this is a helper function to generate complete binary RMAP packet dumps
 *
 * @param blob the blob buffer; if NULL, the function returns the needed size
 *
 * @param[in]  cmd an rmap command buffer
 * @param[in]  cmd_size the size of the rmap command buffer
 * @param[in]  non_crc_bytes leading bytes in the header not path of the CRC
 * @param[in]  data a data buffer (may be NULL)
 * @param[in]  data_size the size of the data buffer (ignored if data is NULL)
 *
 * @returns the size of the blob or 0 on error
 */

int fee_sim_package(uint8_t *blob,
		    const uint8_t *cmd,  int cmd_size,
		    const uint8_t non_crc_bytes,
		    const uint8_t *data, int data_size)
{
	int n;
	int has_data_crc = 0;
	struct rmap_instruction *ri;



	if (!cmd_size) {
		blob = NULL;
		return 0;
	}


	/* allocate space for header, header crc, data, data crc */
	n = cmd_size + 1;

	ri = (struct rmap_instruction *) &cmd[non_crc_bytes + RMAP_INSTRUCTION];

	/* see if the type of command needs a data crc field at the end */
	switch (ri->cmd) {
	case RMAP_READ_ADDR_SINGLE:
	case RMAP_READ_ADDR_INC:
		has_data_crc = 1;
		n += 1;
		break;
	default:
		break;
	}


	if (data)
		n += data_size;

	if (!blob)
		return n;


	memcpy(&blob[0], cmd, cmd_size);

	blob[cmd_size] = rmap_crc8(&cmd[non_crc_bytes],
				   cmd_size - non_crc_bytes);

	if (data) {
		memcpy(&blob[cmd_size + 1], data, data_size);
		blob[cmd_size + 1 + data_size] = rmap_crc8(data, data_size);
	} else {
		/* if no data is present, data crc is 0x0 */
		if (has_data_crc)
			blob[cmd_size + 1] = 0x0;
	}


	return n;
}

/* simulate FEE receiving data */
static void rmap_sim_rx(uint8_t *pkt)
{
	int i, n;

	uint8_t src;

	uint8_t *hdr;
	uint8_t *data = NULL;

	int hdr_size;
	int data_size = 0;

	struct rmap_pkt *rp;



	rp = rmap_pkt_from_buffer(pkt);

	if (!rp) {
		printf("conversion error!");
		exit(0);
	}


	/* The rmap libary does not implement client mode, so we do the
	 * basics here.
	 * At the moment, we only use
	 *	RMAP_READ_ADDR_INC and
	 *	RMAP_WRITE_ADDR_INC_VERIFY_REPLY,
	 * because we only implemented the config registers, so this is pretty
	 * easy
	 */


	/* we reuse the packet, turn it into a response */
	rp->ri.cmd_resp = 0;

	/* WARNING: path addressing requires extra steps (leading zero removal),
	 * this not done because it's unused in this demo
	 */

	/* flip around logical addresses */
	src = rp->src;
	rp->src = rp->dst;
	rp->dst = src;

	switch (rp->ri.cmd) {
		case RMAP_READ_ADDR_INC:
			printf("RMAP_READ_ADDR_INC\n");
			printf("read from addr: %x, size %d \n", rp->addr, rp->data_len);

			data = malloc(rp->data_len);
			if (!data) {
				printf("error allocating buffer\n");
				exit(0);
			}

			data_size = rp->data_len;

			/* copy the simulated register map into the data payload
			 * This works because the register map in the FEE
			 * starts at address 0x0
			 */
			memcpy(data, (&((uint8_t *) &smile_fee_mem)[rp->addr]), data_size);

			break;

		case RMAP_WRITE_ADDR_INC_VERIFY_REPLY:
			printf("RMAP_WRITE_ADDR_INC_VERIFY_REPLY\n");
			printf("write to addr: %x, size %d \n", rp->addr, rp->data_len);
			/* copy the payload into the simulated register
			 * map. This works because the register map in the FEE
			 * starts at address 0x0
			 */
			memcpy((&((uint8_t *) &smile_fee_mem)[rp->addr]), rp->data, rp->data_len);

			rp->data_len = 0; /* no data in reply */

			break;
		default:
			printf("rmap command not implemented: %x\n", rp->ri.cmd);
			break;
	}

	/* convert packet to buffer */

	/* determine header size */
	hdr_size = rmap_build_hdr(rp, NULL);

	hdr = malloc(hdr_size);
	if (!hdr) {
		printf("Error allocating memory\n");
		exit(0);
	}

	rmap_build_hdr(rp, hdr);

	rmap_erase_packet(rp);


	/* convert to blob and store in our reply array */

	for (i = 0; i < 64; i++)  {
		if (!rmap_sim_reply[i])
			break; /* found unused slot */
	}

	if (i > 64) {
		printf("Error, out of transmit slots\n");
		exit(0);
	}


	/* determine required buffer size
	 * WARNING non-crc bytes are hardcoded to 0 since we do not use path
	 * addressing in this demo
	 */
	n = fee_sim_package(NULL, hdr, hdr_size, 0,
			    data, data_size);

	rmap_sim_reply[i] = malloc(n);
	rmap_sim_reply_size[i] = n;

	if (!rmap_sim_reply[i]) {
		printf("Error allcating blob!\n");
		exit(0);
	}

	/* copy packet data to buffer */
	smile_fee_package(rmap_sim_reply[i], hdr, hdr_size, 0,
			  data, data_size);

}


/* simulate FEE sending data */

static int rmap_sim_tx(uint8_t *pkt)
{
	int i;
	int size;
	static int last = 65; /* out of bounds */


	/* simulate some activity here by clearing the execute op flag if it
	 * is set.
	 */

	if (smile_fee_mem.cfg_reg_24 & 0x1UL) {
		smile_fee_mem.cfg_reg_24 &= ~0x1UL;
	}


	/* actual tx simulation */

	if (last > 64) {

		for (i = 0; i < 64; i++)  {
			if (rmap_sim_reply[i])
				break; /* found used slot */
		}

		if (i >= 64)
			return 0;

		last = i; /* our current reply packet */

	}

	/* was only a size request */
	if (!pkt)
		return rmap_sim_reply_size[last];

	/* now deliver contents */
	size = rmap_sim_reply_size[last];
	memcpy(pkt, rmap_sim_reply[last], size);

	free(rmap_sim_reply[last]);

	/* mark slot as unused */
	rmap_sim_reply[last] = NULL;

	/* reset to ouf of bounds */
	last = 65;

	return size;
}


/**
 * tx function for smile_fee_ctrl
 *
 * @note you may want to reimplement this function if you use a different
 *	 SpaceWire interface or if you want transport/dump the RMAP
 *	 packets via a different mechanism, e.g. using smile_fee_package()
 *
 * @warn If you use smile_fee_package() to simply dump generated RMAP command
 *	 packets, you may run into the limit set by TRANS_LOG_SIZE, as
 *	 transactions make an entry in the transaction log, which will only
 *	 free up slots when an ACK with the  corresponding RMAP transaction id
 *	 has been received. So, if you simply want to dump a set of commands,
 *	 and run into issues, increase TRANS_LOG_SIZE by an arbitrary value
 */

static int32_t rmap_tx(const void *hdr,  uint32_t hdr_size,
		       const uint8_t non_crc_bytes,
		       const void *data, uint32_t data_size)
{
#if 1
	int pkt_size;
	uint8_t *blob ;


	/* determine required buffer size */
	pkt_size = smile_fee_package(NULL, hdr, hdr_size, non_crc_bytes,
				     data, data_size);

	blob = malloc(pkt_size);

	if (!blob) {
		printf("Error allcating blob!\n");
		return -1;
	}

	/* copy packet data to buffer */
	pkt_size = smile_fee_package(blob, hdr, hdr_size, non_crc_bytes,
				     data, data_size);

#if 0
	/* print it */
	for (i = 0; i < pkt_size; i++)
	       printf("%02x:", blob[i]);
	printf("\n");
#endif
	/* "send" to FEE */
	rmap_sim_rx(blob);

	free(blob);

	return 0;

#else
	/* adapt to IASW like this */
	CrFwPckt_t pckt; /* <- allocate and copy hdr and data into that */
	/* maybe use smile_fee_package() for that) */
	CrIbFeePcktHandover(CrFwPckt_t pckt)
	return 0;
#endif
}


/**
 * rx function for smile_fee_ctrl
 *
 * @note you may want to reimplement this function if you use a different
 *	 SpaceWire interface or if you want inject RMAP packets via a
 *	 different mechanism
 */

static uint32_t rmap_rx(uint8_t *pkt)
{
#if 1
	return rmap_sim_tx(pkt);

#else
	/* adapt to IASW like this */



	/* WARNING: Where do I geht the packet size from when used with the
	 *	    IASW? I guess it's CrFwPcktGetLength();
	 *	    The underlying library expects the size of the
	 *	    next packet to be returned when the pkt argument is NULL,
	 *	    i.e. rmap_rx(NULL) must return the packet size and hold
	 *	    the packet until it is called with a valid buffer.
	 *	    Note that these calls will be immediate within the same
	 *	    function, so you won't need to implement an actual buffer
	 *	    mechanism. I suppose if you CrIbFeePcktCollect here
	 *	    and store the packet reference in a static variable within
	 *	    the function, then return the size if pkt == NULL and
	 *	    copy it when this function is called with a pointer, you
	 *	    should be fine.
	 *
	 *	    i.e. do something like this:
	 *	    (I suppose this can be simplified)
	 */

	static CrFwPckt_t fw_pckt;

	/* size request */
	if (!pkt) {
		if (fw_pckt)	/* pending packet */
			return CrFwPcktGetLength(fw_pckt); /* I guess... */
	}

	if (!fw_pckt) { /* have nothing */
		if (!CrIbIsFeePcktAvail(CR_FW_CLIENT_FEE))
			return 0;	/* no packet */
	}

	if (!fw_pckt) /* nothing stored retrieve new */
		fw_pckt = CrIbFeePcktCollect(CR_FW_CLIENT_FEE);

	pkt_size = CrFwPcktGetLength(fw_pckt);

	if (pkt) {	/* If we got a pointer, assume we can do that */
		memcpy(pkt, fw_pckt, pkt_size);

		/* free fw_pckt here (if needed) and set to NULL */
		fw_pckt = NULL
	}

	return pkt_size;
#endif
}


/**
 * @brief save repeating 3 lines of code..
 *
 * @note prints abort message if pending status is non-zero after 10 retries
 */

static void sync(void)
{
	int cnt = 0;
	printf("syncing...");
	while (smile_fee_rmap_sync_status()) {
		printf("pending: %d\n", smile_fee_rmap_sync_status());

		if (cnt++ > 10) {
			printf("aborting; de");
			break;
		}

	}
	printf("synced\n");
}



/**
 * SMILE FEE commanding demonstrator, this would run on the DPU
 * note: all values are random
 */

static void smile_fee_demo(void)
{

	printf("Configuring start of vertical row shared with charge injection\n");
	smile_fee_set_vstart(35);

	printf("Configuring duration of a parallel overlap period (TOI)\n");
	smile_fee_set_parallel_toi_period(5);


	printf("Syncing configured values to FEE via RMAP\n");
	smile_fee_sync_vstart(DPU2FEE);
	smile_fee_sync_parallel_toi_period(DPU2FEE);

	printf("Waiting for sync to complete\n");
	sync();

	printf("Verifying configured values in FEE\n");

	printf("Clearing local values\n");
	smile_fee_set_vstart(0);
	smile_fee_set_parallel_toi_period(0);

	printf("Syncing configured values from FEE to DPU via RMAP\n");
	smile_fee_sync_vstart(FEE2DPU);
	smile_fee_sync_parallel_toi_period(FEE2DPU);

	printf("Waiting for sync to complete\n");
	sync();


	printf("Checking values: vertical row shared with charge injection: ");

	if (smile_fee_get_vstart() == 35)
		printf("SUCCESS\n");
	else
		printf("FAILURE\n");


	printf("Checking values: duration of a parallel overlap period (TOI): ");

	if (smile_fee_get_parallel_toi_period() == 5)
		printf("SUCCESS\n");
	else
		printf("FAILURE\n");


	printf("Setting execute op flag to expedite operational parameters\n");
	smile_fee_set_execute_op(1);

	printf("Syncing execute op flag to FEE via RMAP\n");
	smile_fee_sync_execute_op(DPU2FEE);
	printf("Waiting for sync to complete\n");
	sync();


	printf("Waiting for FEE to complete operation\n");

	while (1) {
		printf("Syncing execute op flag from FEE to DPU via RMAP\n");
		smile_fee_sync_execute_op(FEE2DPU);
		printf("Waiting for sync to complete\n");
		sync();

		if (!smile_fee_get_execute_op())
			break;

		printf("FEE hast not yet completed operation\n");
	}


	printf("FEE operation completed\n");
}





int main(void)
{
#if 0
	uint8_t dpath[] = DPATH;
	uint8_t rpath[] = RPATH;
#endif

	/* initialise the libraries */
	smile_fee_ctrl_init();
	smile_fee_rmap_init(GRSPW2_DEFAULT_MTU, rmap_tx, rmap_rx);


	/* configure rmap link (adapt values as needed) */

	smile_fee_set_source_logical_address(DPU_ADDR);
	smile_fee_set_destination_key(FEE_DEST_KEY);
	smile_fee_set_destination_logical_address(FEE_ADDR);
	smile_fee_set_destination_path(NULL, 0);
	smile_fee_set_return_path(NULL, 0);

	/* now run the demonstrator */
	smile_fee_demo();
}
