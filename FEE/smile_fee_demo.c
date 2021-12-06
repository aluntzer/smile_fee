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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/tcp.h>

#include <gresb.h>

#include <smile_fee.h>
#include <smile_fee_cfg.h>
#include <smile_fee_ctrl.h>
#include <smile_fee_rmap.h>

#include <rmap.h>	/* for FEE simulation */

/* whatever for now ... */
#define MAX_PAYLOAD_SIZE	4096

/* XXX include extra for RMAP headers, 128 bytes is plenty */
#undef GRSPW2_DEFAULT_MTU
#define GRSPW2_DEFAULT_MTU (MAX_PAYLOAD_SIZE + 128)

int bridge_fd;


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
__attribute__((unused))
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
#ifdef DEBUG
			printf("RMAP_READ_ADDR_INC\n");
			printf("read from addr: %x, size %d \n", rp->addr, rp->data_len);
#endif
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
#ifdef DEBUG
			printf("RMAP_WRITE_ADDR_INC_VERIFY_REPLY\n");
			printf("write to addr: %x, size %d \n", rp->addr, rp->data_len);
#endif
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
__attribute__((unused))
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
	int pkt_size;
	uint8_t *blob;

	uint8_t *gresb_pkt __attribute__((unused));

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

#ifdef FEE_SIM

	/* "send" to FEE */
	rmap_sim_rx(blob);
#else

	/* encapsulate in GRESB packet and send */
	gresb_pkt = gresb_create_host_data_pkt(blob, pkt_size);

	if(send(bridge_fd, gresb_pkt, gresb_get_host_data_pkt_size(gresb_pkt), 0) < 0) {
		perror("Send failed");
		return -1;
	}

	gresb_destroy_host_data_pkt((struct host_to_gresb_pkt *) gresb_pkt);


	free(blob);
#endif

#if 0
	/* adapt to IASW like this */
	CrFwPckt_t pckt; /* <- allocate and copy hdr and data into that */
	/* maybe use smile_fee_package() for that) */
	CrIbFeePcktHandover(CrFwPckt_t pckt)
#endif

	return 0;
}


/**
 * dirty hack for packet reception, more or less a copy of rmap_rx()
 */

static uint32_t pkt_rx(uint8_t *pkt)
{
	int recv_bytes;
	static uint32_t pkt_size; /* keep last packet size */

	/* XXX: gresb-to-host header is just 4 bytes, but we need 2 extra in
	 * order to be able to distinguish between rmap and non-rmap packets
	 *
	 *
	 * NOTE THAT THIS IS A DIRTY HACK FOR THE DEMONSTRATOR ONLY, DO NOT DO
	 * THIS IN PRODUCTION CODE! EVER!
	 */

	uint8_t gresb_hdr[4+2];

	uint8_t *recv_buffer;

	if (!pkt) {	/* next packet size requested */

		/* try to grab a header */
		recv_bytes = recv(bridge_fd, gresb_hdr, 6, MSG_PEEK | MSG_DONTWAIT);

		/* we won't bother, this is a stupid demo, not production code */
		if (recv_bytes <= 0)
			return 0;

		/* header is 4 bytes, but we need 6 */
		if (recv_bytes < (4 + 2))
			return 0;

		pkt_size = gresb_get_spw_data_size(gresb_hdr);

		/* XXX the protocol id is (or should be) in byte 6 */
		if (gresb_hdr[5] != FEE_DATA_PROTOCOL)
			return 0;

		/* tell caller about next packet */
		return pkt_size;
	}

	/* we packet space, now start receiving
	 * note the lack of basic sanity checks...
	 */

	/* buffer is payload + header */
	recv_buffer = malloc(pkt_size + 4);

	recv_bytes  = recv(bridge_fd, recv_buffer, pkt_size + 4, 0);


	/* the caller supplied their own buffer */
	memcpy(pkt, gresb_get_spw_data(recv_buffer), pkt_size);
	free(recv_buffer);

	return pkt_size;

}



/**
 * rx function for smile_fee_ctrl
 *
 * @note you may want to reimplement this function if you use a different
 *	 SpaceWire interface or if you want inject RMAP packets via a
 *	 different mechanism
 * @note pkt ist allocated by the caller
 */

static uint32_t rmap_rx(uint8_t *pkt)
{

#if FEE_SIM
	return rmap_sim_tx(pkt);
#else
	int recv_bytes;
	static uint32_t pkt_size; /* keep last packet size */

	/* XXX: gresb-to-host header is just 4 bytes, but we need 2 extra in
	 * order to be able to distinguish between rmap and non-rmap packets
	 *
	 *
	 * NOTE THAT THIS IS A DIRTY HACK FOR THE DEMONSTRATOR ONLY, DO NOT DO
	 * THIS IN PRODUCTION CODE! EVER!
	 */

	uint8_t gresb_hdr[4+2];

	uint8_t *recv_buffer;

	if (!pkt) {	/* next packet size requested */

		/* try to grab a header */
		recv_bytes = recv(bridge_fd, gresb_hdr, 6, MSG_PEEK | MSG_DONTWAIT);

		/* we won't bother, this is a stupid demo, not production code */
		if (recv_bytes <= 0)
			return 0;

		/* header is 4 bytes, but we need 6 */
		if (recv_bytes < (4 + 2))
			return 0;

		pkt_size = gresb_get_spw_data_size(gresb_hdr);

		/* XXX the protocol id is (or should be) in byte 6 */
		if (gresb_hdr[5] != RMAP_PROTOCOL_ID)
			return 0;

		/* tell caller about next packet */
		return pkt_size;
	}

	/* we packet space, now start receiving
	 * note the lack of basic sanity checks...
	 */

	/* buffer is payload + header */
	recv_buffer = malloc(pkt_size + 4);

	recv_bytes  = recv(bridge_fd, recv_buffer, pkt_size + 4, 0);


	/* the caller supplied their own buffer */
	memcpy(pkt, gresb_get_spw_data(recv_buffer), pkt_size);
	free(recv_buffer);

	return pkt_size;
#endif


#if 0
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

static void sync_rmap(void)
{
	int cnt = 0;
	printf("\nsyncing...");
	while (smile_fee_rmap_sync_status()) {
		usleep(10000);
#if DEBUG
		printf("pending: %d\n", smile_fee_rmap_sync_status());
#endif

		if (cnt++ > 10) {
			printf("aborting; de");
			break;
		}

	}
	printf("synced\n\n");
}

/**
 *  procedure Test 1: read a basic FEE register
 *
 */

static void smile_fee_test1(void)
{
	printf("Test1: read a basic FEE register\n");

	printf("sync vstart/vend from FEE\n");
	smile_fee_sync_vstart(FEE2DPU);
	sync_rmap();

	printf("vstart: %x, vend %x\n", smile_fee_get_vstart(), smile_fee_get_vend());

	printf("Test1 complete\n\n");
}


/**
 *  procedure Test 2: read, write & read a basic FEE register
 *
 */

static void smile_fee_test2(void)
{
	printf("Test 2: read, write & read a basic FEE register\n");

	printf("sync ccd2 e/f single pixel threshold from FEE\n");
	smile_fee_sync_ccd2_e_pix_treshold(FEE2DPU);

	sync_rmap();

	printf("ccd2 e value currently: %x\n", smile_fee_get_ccd2_e_pix_treshold());
	printf("ccd2 f value currently: %x\n", smile_fee_get_ccd2_f_pix_treshold());
	printf("setting2 ccd e/f local values\n");

	smile_fee_set_ccd2_e_pix_treshold(0x7b);
	smile_fee_set_ccd2_f_pix_treshold(0x7c);

	printf("ccd2 e local value now: %x\n", smile_fee_get_ccd2_e_pix_treshold());
	printf("ccd2 f local value now: %x\n", smile_fee_get_ccd2_f_pix_treshold());

	printf("syncing ccd2 e/f single pixel threshold to FEE\n");
	smile_fee_sync_ccd2_e_pix_treshold(DPU2FEE);

	sync_rmap();

	printf("clearing local values for verification\n");
	smile_fee_set_ccd2_e_pix_treshold(0x0);
	smile_fee_set_ccd2_f_pix_treshold(0x0);

	printf("syncing back ccd2 e/f single pixel threshold from FEE\n");
	smile_fee_sync_ccd2_e_pix_treshold(FEE2DPU);

	sync_rmap();

	printf("ccd1 value now: %x\n", smile_fee_get_ccd2_e_pix_treshold());
	printf("ccd2 value now: %x\n", smile_fee_get_ccd2_f_pix_treshold());

	printf("Test2 complete\n\n");
}


/**
 *  procedure Test 3: Get 6x6 binned pattern images from "Frame Transfer Pattern
 *  Mode."
 *
 */


static void smile_fee_test3(void)
{
	printf("Test 3: 6x6 binned pattern from frame transfer pattern mode\n");


	smile_fee_set_packet_size(0x30c);
	smile_fee_set_int_period(0x0fa0);

	/* all above are reg4, this will suffice */
	smile_fee_sync_packet_size(DPU2FEE);

	smile_fee_set_correction_bypass(1);
	smile_fee_set_digitise(1);
	smile_fee_set_readout_nodes(3);

	/* all above are reg5, this will suffice */
	smile_fee_sync_correction_bypass(DPU2FEE);

	smile_fee_set_ccd_mode_config(0x1);
	smile_fee_set_ccd_mode2_config(0x2);

	/* all above are reg32, this will suffice */
	smile_fee_sync_ccd_mode_config(DPU2FEE);

	sync_rmap(); /* make sure all parameters are set */

	/* trigger packet transmission */
	smile_fee_set_execute_op(0x1);
	smile_fee_sync_execute_op(DPU2FEE);

	sync_rmap();

	while (1)
	{
		static int ps = 1;	/* times to print everything... */
		static int pp = 1;	/* times to print everything... */
		int n, i;
		struct fee_data_pkt *pkt;
		struct fee_pattern  *pat;

		usleep(1000);

		n  = pkt_rx(NULL);

		if (n)
			pkt = (struct fee_data_pkt *) malloc(n);
		else
			continue;

		n = pkt_rx((uint8_t *) pkt);

		if (n <= 0)
			printf("Error in pkt_rx()\n");


		pkt->hdr.data_len   = __be16_to_cpu(pkt->hdr.data_len);
		pkt->hdr.frame_cntr = __be16_to_cpu(pkt->hdr.frame_cntr);
		pkt->hdr.seq_cntr   = __be16_to_cpu(pkt->hdr.seq_cntr);

		if (ps) {
			ps--;

			printf("data type %d len %d frame %d seq %d\n",
			       pkt->hdr.type.pkt_type,
			       pkt->hdr.data_len,
			       pkt->hdr.frame_cntr,
			       pkt->hdr.seq_cntr);
		}

		pat = (struct fee_pattern *) &pkt->data;
		n = pkt->hdr.data_len / sizeof(struct fee_pattern);

		if (pp) {
			pp--;
			printf("n %d\n", n);
			for (i = 0; i < n; i++) {
				printf("%d %d %d %d %d\n", pat[i].time_code, pat[i].ccd, pat[i].side, pat[i].row, pat[i].col);

			}
		}

		/* setup abort ... */
		if (pkt->hdr.seq_cntr == 2555)	/* gen stops about there? */
			ps = -1;

		free(pkt);

		/* abort ... */
		if (ps < 0)
			break;
	}

	printf("Test3 complete\n\n");
}


/**
 * SMILE FEE commanding demonstrator, this would run on the DPU
 */

static void smile_fee_demo(void)
{
	smile_fee_test1();
	smile_fee_test2();
	smile_fee_test3();


	printf("standing by\n");
	while(1) usleep(1000000); /* stop here for now */
}



int main(void)
{
	uint8_t dpath[] = DPATH;
	uint8_t rpath[] = RPATH;

#if !defined (FEE_SIM)
	int flag = 1;
	struct sockaddr_in server;


	bridge_fd = socket(AF_INET, SOCK_STREAM, 0);

	setsockopt(bridge_fd, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(int));

	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons(1234);


	if (connect(bridge_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
		perror("connect failed. Error");
		return 1;
	}

	/* set non-blocking so we can recv() easily */
	fcntl(bridge_fd, F_SETFL, fcntl(bridge_fd, F_GETFL, 0) | O_NONBLOCK);
#endif

	/* initialise the libraries */
	smile_fee_ctrl_init(NULL);
	smile_fee_rmap_init(GRSPW2_DEFAULT_MTU, rmap_tx, rmap_rx);


	/* configure rmap link (adapt values as needed) */

	smile_fee_set_source_logical_address(DPU_ADDR);
	smile_fee_set_destination_key(FEE_DEST_KEY);
	smile_fee_set_destination_logical_address(FEE_ADDR);
	smile_fee_set_destination_path(dpath, DPATH_LEN);
	smile_fee_set_return_path(rpath, RPATH_LEN);

	/* now run the demonstrator */
	smile_fee_demo();
}
