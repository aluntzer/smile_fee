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

#include <sys/time.h>

#include <gresb.h>

#include <smile_fee.h>
#include <smile_fee_cfg.h>
#include <smile_fee_ctrl.h>
#include <smile_fee_rmap.h>

#include <fitsio.h>

/* whatever for now ... */
#define MAX_PAYLOAD_SIZE	4096

/* XXX include extra for RMAP headers, 128 bytes is plenty */
#undef GRSPW2_DEFAULT_MTU
#define GRSPW2_DEFAULT_MTU (MAX_PAYLOAD_SIZE + 128)

int bridge_fd;



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

	uint8_t *gresb_pkt;

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


	/* encapsulate in GRESB packet and send */
	gresb_pkt = gresb_create_host_data_pkt(blob, pkt_size);

	if(send(bridge_fd, gresb_pkt, gresb_get_host_data_pkt_size(gresb_pkt), 0) < 0) {
		perror("Send failed");
		return -1;
	}

	gresb_destroy_host_data_pkt((struct host_to_gresb_pkt *) gresb_pkt);


	free(blob);

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
	int recv_left;
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
		if (recv_bytes <= 0)  {
			return 0;
		}

		/* header is 4 bytes, but we need 6 */
		if (recv_bytes < (4 + 2)) {
			return 0;
		}

		/* XXX the protocol id is (or should be) in byte 6 */
		if (gresb_hdr[5] == FEE_DATA_PROTOCOL)
			pkt_size = gresb_get_spw_data_size(gresb_hdr);
		else	/* not what we want, ignore */
			pkt_size = 0;

		/* tell caller about next packet */
		return pkt_size;
	}

	/* we packet space, now start receiving
	 * note the lack of basic sanity checks...
	 */

	/* buffer is payload + header */
	recv_buffer = malloc(pkt_size + 4);

	recv_bytes = 0;
	recv_left  = pkt_size + 4;
	while (recv_left) {
		int rb;
		rb = recv(bridge_fd, recv_buffer + recv_bytes, recv_left, 0);
		recv_bytes += rb;
		recv_left  -= rb;
	}


	if (0) {
		int i;

		printf("\nRAW DUMP (%d/%d bytes):\n", recv_bytes, pkt_size);

		for (i=0 ;i < recv_bytes; i++)
			printf("%02x ", recv_buffer[i]);
		printf("END DUMP\n\n");
	}



	/* the caller supplied their own buffer */
	memcpy(pkt, gresb_get_spw_data(recv_buffer), recv_bytes);
	free(recv_buffer);

	return recv_bytes;

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
	int recv_left;
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

		/* XXX the protocol id is (or should be) in byte 6 */
		if (gresb_hdr[5] == RMAP_PROTOCOL_ID)
			pkt_size = gresb_get_spw_data_size(gresb_hdr);
		else
			pkt_size = 0;

		/* tell caller about next packet */
		return pkt_size;
	}

	/* we packet space, now start receiving
	 * note the lack of basic sanity checks...
	 */

	/* buffer is payload + header */
	recv_buffer = malloc(pkt_size + 4);

	recv_bytes = 0;
	recv_left  = pkt_size + 4;
	while (recv_left) {
		int rb;
		rb = recv(bridge_fd, recv_buffer + recv_bytes, recv_left, 0);
		recv_bytes += rb;
		recv_left  -= rb;
	}


	/* the caller supplied their own buffer */
	memcpy(pkt, gresb_get_spw_data(recv_buffer), recv_bytes);
	free(recv_buffer);

	return recv_bytes;
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
 *  procedure Test 1: Smile Test Plan Will_SS_V0.1 Verification No 1
 *
 *  in On-Mode, configure FT pattern mode, binning at 6x6, packet size 778
 *  all other parameters left at default
 *
 *  once, configured, set execute_op
 *
 *  expected reply: HK, followed by pattern data
 *
 */


static void smile_fee_test1(void)
{
	struct timeval t0, t;
	double elapsed_time;

#define FITS 1
#if FITS
	size_t off = 0;
	long naxes[3];
	long n_elem;
	int status = 0;
	fitsfile *ff;
	uint16_t *buf;
	long fpixel[3] = {1, 1, 1};
#endif /* FITS */

	printf("Test 1: 6x6 binned pattern from frame transfer pattern mode\n");


	/* update local configuration of all used registers
	 * note: we naively issue syncs for all the register field names
	 * as we use them; many of these are actually part of the same
	 * register, but this way, we don't have to know and just
	 * create a few more RMAP transfers.
	 */
	smile_fee_sync_packet_size(FEE2DPU);
	smile_fee_sync_int_period(FEE2DPU);
	smile_fee_sync_readout_node_sel(FEE2DPU);
	smile_fee_sync_ccd_mode_config(FEE2DPU);
	smile_fee_sync_readout_node_sel(FEE2DPU);
	smile_fee_sync_ccd_mode_config(FEE2DPU);
	smile_fee_sync_ccd_mode2_config(FEE2DPU);
	smile_fee_sync_execute_op(FEE2DPU);

	/* flush all pending transfers */
	sync_rmap();


	/* packet size = 778 */
	smile_fee_set_packet_size(0x030A);

	/* derived from reg. value specified in test plan */
	smile_fee_set_int_period(0x0FA0);

	/*
	 * these should not be needed as they are the default configuration;
	 * enable in case of no transfer
	 * (digitise must be enabled to actually transfer data to the DPU)
	 */
#if 0
	smile_fee_set_correction_bypass(1);
	smile_fee_set_digitise_en(1);
#endif
	/* this is not explicitly specified in the test, but all nodes
	 * must be set, otherwise no pattern is generated (known bug (?))
	 */
	smile_fee_set_readout_node_sel(0xF);


	/* set frame transfer pattern mode */
	smile_fee_set_ccd_mode_config(0x3);

	/* set 6x6 binning */
	smile_fee_set_ccd_mode2_config(0x2);



	/* same as above, just use the _sync_ calls for the
	 * particular fields
	 */
	smile_fee_sync_packet_size(DPU2FEE);
	smile_fee_sync_int_period(DPU2FEE);
	smile_fee_sync_readout_node_sel(DPU2FEE);
	smile_fee_sync_ccd_mode_config(DPU2FEE);
	smile_fee_sync_readout_node_sel(DPU2FEE);
	smile_fee_sync_ccd_mode_config(DPU2FEE);
	smile_fee_sync_ccd_mode2_config(DPU2FEE);

	/* flush all pending transfers */
	sync_rmap();


	/* now trigger the operation, we do this in a separate transfer */
	smile_fee_set_execute_op(0x1);
	smile_fee_sync_execute_op(DPU2FEE);

	sync_rmap();	/* flush */

	/* for our (approximate) transfer time profile */
	gettimeofday(&t0, NULL);

#if FITS
	naxes[0] = FEE_EDU_FRAME_6x6_COLS;
	naxes[1] = FEE_EDU_FRAME_6x6_ROWS;
	naxes[2] = 4; /* 2 ccds, 4 sides */
	n_elem = naxes[0] * naxes[1] * naxes[2];
	off = 0;
	if (fits_create_file(&ff, "!dump.fits", &status)) {
		printf("error %d\n", status);
		exit(-1);
	}

	buf = malloc(n_elem * sizeof(uint16_t));
	if (!buf) {
		perror("malloc");
		exit(-1);
	}

	if (fits_create_img(ff, USHORT_IMG, 3, naxes, &status)) {
		printf("error %d\n", status);
		exit(-1);
	}


#endif /* FITS */

	while (1)
	{
		static int ps = 1;
		static int pp = 0;
		static int pckt_type_seq_cnt;

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


		gettimeofday(&t, NULL);

		/* time in ms since execute_op */
		elapsed_time  = (t.tv_sec  - t0.tv_sec)  * 1000.0;
		elapsed_time += (t.tv_usec - t0.tv_usec) / 1000.0;

		pkt->hdr.data_len	= __be16_to_cpu(pkt->hdr.data_len);
		pkt->hdr.fee_pkt_type   = __be16_to_cpu(pkt->hdr.fee_pkt_type);
		pkt->hdr.frame_cntr	= __be16_to_cpu(pkt->hdr.frame_cntr);
		pkt->hdr.seq_cntr	= __be16_to_cpu(pkt->hdr.seq_cntr);

		if (ps) {
			ps--;

			printf("data type %d %x len %d frame %d seq %d last: %d side %d id %d fee_mode %d time %g ms\n",
			       pkt->hdr.type.pkt_type,
			       pkt->hdr.fee_pkt_type &0x3,
			       pkt->hdr.data_len,
			       pkt->hdr.frame_cntr,
			       pkt->hdr.seq_cntr,
			       pkt->hdr.type.last_pkt,
			       pkt->hdr.type.ccd_side,
			       pkt->hdr.type.ccd_id,
			       pkt->hdr.type.fee_mode,
			       elapsed_time);
		}

		if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_DATA) {

			pat = (struct fee_pattern *) &pkt->data;
			n = pkt->hdr.data_len / sizeof(struct fee_pattern);

			if (pp) {
				pp--;
				for (i = 0; i < n; i++) {
					printf("TC:%02x CCD:%02x SIDE:%02x ROW:%02x COL:%02x RAW: %04x\n",
					       pat[i].time_code, pat[i].ccd, pat[i].side,
					       pat[i].row, pat[i].col, pat[i].field );
				}
			}

#if FITS
			if ((ssize_t) off - pkt->hdr.data_len < n_elem) {
				memcpy(&buf[off], &pkt->data, pkt->hdr.data_len);
				off+= pkt->hdr.data_len;
			}
#endif

		} else if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_HK) {
			if (ps)
				printf("This is HK data, not printing\n");

		} else {
			printf("unknown type %d\n", pkt->hdr.fee_pkt_type );
		}


		/* dunno if change of "Table 8-12 Packetised FT mode
		 * sequence counter" is already implmented;
		 * in this case, the packet types transmitted would be
		 * HK, E2, F2, E4, F4, each terminated with the bit
		 * set high. Note that this implies that we have to
		 * check the particular readout mode; since
		 * currently all sides/ccds are (and must be) selected
		 * in this transfer (see above), this means that we get
		 * a total of 5 "last_pkt" bits set hi
		 */
		if (pkt->hdr.type.last_pkt) {
			printf("LAST_PCKT: %g ms, size %d\n", elapsed_time, pkt->hdr.data_len);
			if (pckt_type_seq_cnt++ >= 4)
				ps = -1;	/* set loop abort */
		}


		free(pkt);

		/* abort ... */
		if (ps < 0)
			break;
	}


	if (fits_write_pix(ff, TUSHORT, fpixel, n_elem, buf, &status)) {
		printf("error %d (%d)\n",status, __LINE__);
		exit(-1);
	}

	if (fits_close_file(ff, &status)) {
		printf("error %d (%d)\n",status, __LINE__);
		exit(-1);
	}


	free(buf);

	printf("Test1 complete\n\n");
}




/**
 * SMILE FEE commanding demonstrator, this would run on the DPU
 */

static void smile_fee_demo(void)
{
	smile_fee_test1();

	printf("standing by\n");
	while(1) usleep(1000000); /* stop here for now */
}



int main(void)
{
	uint8_t dpath[] = DPATH;
	uint8_t rpath[] = RPATH;


	int flag = 1;
	struct sockaddr_in server;


	bridge_fd = socket(AF_INET, SOCK_STREAM, 0);

	setsockopt(bridge_fd, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(int));

	//server.sin_addr.s_addr = inet_addr("131.130.51.17");
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons(1234);


	if (connect(bridge_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
		perror("connect failed. Error");
		return 1;
	}

	/* set non-blocking so we can recv() easily */
	fcntl(bridge_fd, F_SETFL, fcntl(bridge_fd, F_GETFL, 0) | O_NONBLOCK);

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
