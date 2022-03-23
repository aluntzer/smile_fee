/**
 *
 * this is a modification of what is in TEST for use with SDP testing
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
		if (cnt++ > 10) {
			printf("aborting; de");
			break;
		}

	}
	printf("synced\n\n");
}




static void save_fits(const char *name, uint16_t *buf, long rows, long cols)
{
	long naxes[3];
	long n_elem;
	int status = 0;
	fitsfile *ff;
	long fpixel[3] = {1, 1, 1};



	naxes[0] = cols;
	naxes[1] = rows;
	naxes[2] = 1;
	n_elem = naxes[0] * naxes[1] * naxes[2];

	if (fits_create_file(&ff, name, &status)) {
		printf("error %d\n", status);
		exit(-1);
	}

	if (fits_create_img(ff, USHORT_IMG, 3, naxes, &status)) {
		printf("error %d\n", status);
		exit(-1);
	}

	if (fits_write_pix(ff, TUSHORT, fpixel, n_elem, buf, &status)) {
		printf("error %d (%d)\n",status, __LINE__);
		exit(-1);
	}

	if (fits_close_file(ff, &status)) {
		printf("error %d (%d)\n",status, __LINE__);
		exit(-1);
	}
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
	struct fee_ft_data *ft;
		struct fee_data_pkt *pkt;


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

	/* derived from reg. value specified in test plan (4s nominal) */
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

	ft = fee_ft_aggregator_create();

	while (1) {

		int n;

#if 0
		usleep(1000);
#endif

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

		fee_pkt_hdr_to_cpu(pkt);



		if (fee_ft_aggregate(ft, pkt) > 0)
			break;

	}

	save_fits("!E2.fits", ft->E2, ft->rows, ft->cols);
	save_fits("!E4.fits", ft->E4, ft->rows, ft->cols);
	save_fits("!F2.fits", ft->F2, ft->rows, ft->cols);
	save_fits("!F4.fits", ft->F4, ft->rows, ft->cols);

	fee_ft_aggregator_destroy(ft);

	free(pkt);
	printf("Test1 complete\n\n");
}




/**
 * run tests
 */

static void smile_fee_run_tests(void)
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

	/* now run the testsuite */
	smile_fee_run_tests();
}
