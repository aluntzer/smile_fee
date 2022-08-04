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

#ifdef SIM_DUMP_FITS
#include <fitsio.h>
#endif

/* restriction from FEE IRD */
#define MAX_PAYLOAD_SIZE	(2048)

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
		printf("Error allocating blob of size %d!\n",pkt_size);
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
//	printf("\nsyncing...");
	while (smile_fee_rmap_sync_status()) {
		usleep(10000);
		if (cnt++ > 10) {
//			printf("aborting; de");
			break;
		}

	}
//	printf("synced\n\n");
}




#ifdef SIM_DUMP_FITS
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
#endif /* SIM_DUMP_FITS */


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
	smile_fee_sync_ccd2_e_pix_threshold(FEE2DPU);

	sync_rmap();

	printf("ccd2 e value currently: %x\n", smile_fee_get_ccd2_e_pix_threshold());
	printf("ccd2 f value currently: %x\n", smile_fee_get_ccd2_f_pix_threshold());
	printf("setting2 ccd e/f local values\n");

	smile_fee_set_ccd2_e_pix_threshold(0x7b);
	smile_fee_set_ccd2_f_pix_threshold(0x7c);

	printf("ccd2 e local value now: %x\n", smile_fee_get_ccd2_e_pix_threshold());
	printf("ccd2 f local value now: %x\n", smile_fee_get_ccd2_f_pix_threshold());

	printf("syncing ccd2 e/f single pixel threshold to FEE\n");
	smile_fee_sync_ccd2_e_pix_threshold(DPU2FEE);

	sync_rmap();

	printf("clearing local values for verification\n");
	smile_fee_set_ccd2_e_pix_threshold(0x0);
	smile_fee_set_ccd2_f_pix_threshold(0x0);

	printf("syncing back ccd2 e/f single pixel threshold from FEE\n");
	smile_fee_sync_ccd2_e_pix_threshold(FEE2DPU);

	sync_rmap();

	printf("ccd1 value now: %x\n", smile_fee_get_ccd2_e_pix_threshold());
	printf("ccd2 value now: %x\n", smile_fee_get_ccd2_f_pix_threshold());

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


	/* Smile Test Plan Will_SS_V0.1 wants 0x0FA0030A for this register,
	 * however the packet size must be 10 + bytes where bytes is a
	 * multiple of 4, so we do that here
	 */
	smile_fee_set_packet_size(0x030C);
	smile_fee_set_int_period(0x0FA0);

	/* all above are reg4, this will suffice */
	smile_fee_sync_packet_size(DPU2FEE);


	/* apparently the reg5 settings are not needed according to
	 * Smile Test Plan Will_SS_V0.1
	 * we keeping as it was once indicated per email
	 * that this was necessary, which appears correct, since digitise
	 * must be enabled to actually transfer data to the DPU
	 */

	smile_fee_set_correction_bypass(1);
	smile_fee_set_digitise_en(1);
	smile_fee_set_readout_node_sel(3);

	/* all above are reg5, this will suffice */
	smile_fee_sync_correction_bypass(DPU2FEE);

	/* frame transfer mode */
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
 *  procedure Test 6: Smile Test Plan Will_SS_V0.1 Verification No 1
 *
 *  in On-Mode, configure FT pattern mode, binning at 6x6, packet size 778
 *  all other parameters left at default
 *
 *  once, configured, set execute_op
 *
 *  expected reply: HK, followed by pattern data
 *
 */


static void smile_fee_test6(void)
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

#ifdef SIM_DUMP_FITS
	save_fits("!E2.fits", ft->E2, ft->rows, ft->cols);
	save_fits("!E4.fits", ft->E4, ft->rows, ft->cols);
	save_fits("!F2.fits", ft->F2, ft->rows, ft->cols);
	save_fits("!F4.fits", ft->F4, ft->rows, ft->cols);
#endif

	fee_ft_aggregator_destroy(ft);

	free(pkt);
	printf("Test1 complete\n\n");
}


/**
 * This test executes FT 6x6 bin mode + event detection
 *
 *  - set E&F pixel thresholds to 1000
 *  - set pixel offset to 100 (0x64)
 *  - set event packet limit to maximum	(16777215, 0xFFFFFF)
 *  - set event detection mode
 *  - disable digitise (no frame transfer)
 *  - set packet size to 778
 *
 *  once, configured, set execute_op
 *
 */

static void smile_fee_test_ev_det_ft(void)
{
#define EV_TEST_DIGITISE 0

	struct timeval t0, t;
	double elapsed_time;
	struct fee_ft_data *ft;
	struct fee_data_pkt *pkt = NULL;
	int ev_cnt = 0;
	FILE *fd;

	printf("Test: FT mode + event detection\n");


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

	/* read both ccd2 and ccd4, E&F sides */
	smile_fee_set_readout_node_sel(0xF);

	/* set EV SIM mode */
	smile_fee_set_ccd_mode_config(FEE_MODE_ID_FT);

	/* set 6x6 binning, required for ED */
	smile_fee_set_ccd_mode2_config(0x2);

	 /* digitise on to enable data transfer */
	smile_fee_set_digitise_en(1);

	/* enable event detection */
	smile_fee_set_event_detection(1);


	/* set pixel thresholds */
	smile_fee_set_ccd2_e_pix_threshold(1000);
	smile_fee_set_ccd2_f_pix_threshold(1000);
	smile_fee_set_ccd4_e_pix_threshold(1000);
	smile_fee_set_ccd4_f_pix_threshold(1000);

	/* set pixel offset */
	smile_fee_set_pix_offset(100);			/* 0x64 */

	/* set maximum number of event packets */
	smile_fee_set_event_pkt_limit(0xFFFFFF);	/* 16777215 */

	smile_fee_sync_parallel_toi_period(DPU2FEE);

	smile_fee_sync_ccd2_e_pix_threshold(DPU2FEE);
	smile_fee_sync_ccd2_f_pix_threshold(DPU2FEE);
	smile_fee_sync_ccd4_e_pix_threshold(DPU2FEE);
	smile_fee_sync_ccd4_f_pix_threshold(DPU2FEE);

	smile_fee_sync_pix_offset(DPU2FEE);
	smile_fee_sync_event_pkt_limit(DPU2FEE);

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
	smile_fee_sync_digitise_en(DPU2FEE);
	smile_fee_sync_event_detection(DPU2FEE);

	/* flush all pending transfers */
	sync_rmap();

	/* now trigger the operation, we do this in a separate transfer */
	smile_fee_set_execute_op(0x1);
	smile_fee_sync_execute_op(DPU2FEE);

	sync_rmap();	/* flush */

	/* for our (approximate) transfer time profile */
	gettimeofday(&t0, NULL);

	ft = fee_ft_aggregator_create();
	fd = fopen("packets.dat", "w");
	while (1) {

		int n;


		if (pkt) {
			free(pkt);
			pkt = NULL;
		}
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

		if (fee_pkt_is_event(pkt)) {
			if (fee_event_is_xray(pkt, 5000, 150*8, 200)) {
				ev_cnt++;
				fwrite((void *)pkt, n, 1, fd);
			}
#if 1
			fee_pkt_show_event(pkt);
				printf("ev_cnt %d\n", ev_cnt);
#endif

			continue; /* for EV_TEST_DIGITISE */
		}

		if (fee_ft_aggregate(ft, pkt) == 1)
			break;

	}
	fclose(fd);
	printf("->>> %d x-ray events classified\n", ev_cnt);

#ifdef SIM_DUMP_FITS
	save_fits("!E2.fits", ft->E2, ft->rows, ft->cols);
	save_fits("!E4.fits", ft->E4, ft->rows, ft->cols);
	save_fits("!F2.fits", ft->F2, ft->rows, ft->cols);
	save_fits("!F4.fits", ft->F4, ft->rows, ft->cols);
#endif

	fee_ft_aggregator_destroy(ft);

	free(pkt);
	printf("End test: FT mode + event detection\n");
}


/**
 *	EV detection sim
 */


static void smile_fee_test789(void)
{
	struct timeval t0, t;
	double elapsed_time;
	struct fee_ft_data *ft;
	struct fee_data_pkt *pkt = NULL;
	int ev_cnt = 0;
	FILE *fd;
	size_t fsize;


	uint16_t *data;

	printf("Test: EV detection sim \n");


#define UPLOAD 0
#if UPLOAD
	/* setup ED sim data in local SRAM copy */


	fd = fopen("../SIM/e_raw.dat", "r");
	if (!fd) {
		perror("e_raw");
		exit(-1);
	}

	fseek(fd, 0, SEEK_END);
	fsize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	if (fsize < FEE_EDU_FRAME_6x6_ROWS * FEE_EDU_FRAME_6x6_COLS * 2 * sizeof(uint16_t)) {
		printf("raw data size must be exactly %ld\n", FEE_EDU_FRAME_6x6_ROWS * FEE_EDU_FRAME_6x6_COLS * 2 * sizeof(uint16_t));
		exit(-1);
	}
	data = malloc(fsize);
	if (!data) {
		perror("malloc");
		exit(-1);
	}

	fread(data, fsize, 1, fd);

	smile_fee_write_sram_16(data, FEE_SRAM_SIDE_E_START, fsize/sizeof(uint16_t));
	fclose(fd);

	fd = fopen("../SIM/f_raw.dat", "r");
	if (!fd) {
		perror("e_raw");
		exit(-1);
	}

	fseek(fd, 0, SEEK_END);
	fsize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	if (fsize < FEE_EDU_FRAME_6x6_ROWS * FEE_EDU_FRAME_6x6_COLS * 2 * sizeof(uint16_t)) {
		printf("raw data size must be exactly 2 * 2 * rows * cols\n");
		exit(-1);
	}
	data = malloc(fsize);
	if (!data) {
		perror("malloc");
		exit(-1);
	}

	fread(data, fsize, 1, fd);


	printf("\nUPLOAD\n");
	smile_fee_write_sram_16(data, FEE_SRAM_SIDE_F_START, fsize/sizeof(uint16_t));
	fclose(fd);

	smile_fee_sync_mirror_to_sram(FEE_SRAM_SIDE_E_START, fsize, smile_fee_get_data_mtu());
	smile_fee_sync_mirror_to_sram(FEE_SRAM_SIDE_F_START, fsize, smile_fee_get_data_mtu());
	sync_rmap();
	printf("\nUPLOAD COMPLETE\n");
#endif /* UPLOAD */

#if 0 /* DOWNLOAD */

	data = calloc(1000, sizeof(uint16_t));

	smile_fee_write_sram_16(data, FEE_SRAM_SIDE_E_START, 1000);
//	smile_fee_read_sram_16(data, FEE_SRAM_SIDE_E_START, 1000);


	printf("E_SIDE:\n");
	for (int i = 0; i < 1000; i++)
		printf("%04x ", data[i]);
	printf("\n");

	printf("\nDOWNLOAD\n");
	smile_fee_sync_sram_to_mirror(FEE_SRAM_SIDE_E_START, 1000 * sizeof(uint16_t), smile_fee_get_data_mtu());
	sync_rmap();
	printf("\nDOWNLOAD COMPLETE\n");

	printf("\n");
	smile_fee_read_sram_16(data, FEE_SRAM_SIDE_E_START, 1000);

	printf("E_SIDE:\n");
	for (int i = 0; i < 1000; i++)
		printf("%04x ", data[i]);
	printf("\n");


#endif

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



	printf("packet size: %d\n", smile_fee_get_packet_size());
	printf("readout node  %d\n", smile_fee_get_readout_node_sel());
	printf("mode config  %d\n", smile_fee_get_readout_node_sel());



	/* flush all pending transfers */
	sync_rmap();


	/* packet size = 778 */
	smile_fee_set_packet_size(0x030A);

	/* derived from reg. value specified in test plan (4s nominal) */
	smile_fee_set_int_period(0x0FA0);

	/* read both ccd2 and ccd4, E&F sides */
	smile_fee_set_readout_node_sel(0xF);

	/* set EV SIM mode */
	smile_fee_set_ccd_mode_config(FEE_MODE_ID_EVSIM);

	/* set 6x6 binning, required for ED */
	smile_fee_set_ccd_mode2_config(0x2);

	 /* digitise on to enable data transfer */
	smile_fee_set_digitise_en(1);

	/* enable event detection */
	smile_fee_set_event_detection(1);


	/* set pixel thresholds */
	smile_fee_set_ccd2_e_pix_threshold(1000);
	smile_fee_set_ccd2_f_pix_threshold(1000);
	smile_fee_set_ccd4_e_pix_threshold(1000);
	smile_fee_set_ccd4_f_pix_threshold(1000);

	/* set pixel offset */
	smile_fee_set_pix_offset(100);			/* 0x64 */

	/* set maximum number of event packets */
	smile_fee_set_event_pkt_limit(0xFFFFFF);	/* 16777215 */

	smile_fee_sync_parallel_toi_period(DPU2FEE);

	smile_fee_sync_ccd2_e_pix_threshold(DPU2FEE);
	smile_fee_sync_ccd2_f_pix_threshold(DPU2FEE);
	smile_fee_sync_ccd4_e_pix_threshold(DPU2FEE);
	smile_fee_sync_ccd4_f_pix_threshold(DPU2FEE);

	smile_fee_sync_pix_offset(DPU2FEE);
	smile_fee_sync_event_pkt_limit(DPU2FEE);

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
	smile_fee_sync_digitise_en(DPU2FEE);
	smile_fee_sync_event_detection(DPU2FEE);

#define WANDERING_MASK_TEST 1
#if WANDERING_MASK_TEST
	smile_fee_set_edu_wandering_mask_en(1);
	smile_fee_sync_edu_wandering_mask_en(DPU2FEE);
#else
	smile_fee_set_edu_wandering_mask_en(0);
	smile_fee_sync_edu_wandering_mask_en(DPU2FEE);
#endif

#define SYNC_SEL_TEST 0
#if SYNC_SEL_TEST
	smile_fee_set_sync_sel(1);
	smile_fee_sync_sync_sel(DPU2FEE);
#else
	smile_fee_set_sync_sel(0);
	smile_fee_sync_sync_sel(DPU2FEE);
#endif


	/* flush all pending transfers */
	sync_rmap();

	/* now trigger the operation, we do this in a separate transfer */
	smile_fee_set_execute_op(0x1);
	smile_fee_sync_execute_op(DPU2FEE);

	sync_rmap();	/* flush */

	/* for our (approximate) transfer time profile */
	gettimeofday(&t0, NULL);

	ft = fee_ft_aggregator_create();
	fd = fopen("packets.dat", "w");
	while (1) {

		int n;


		if (pkt) {
			free(pkt);
			pkt = NULL;
		}
#if 1
		usleep(1000);
#endif

		n  = pkt_rx(NULL);

		if (n) {
			pkt = (struct fee_data_pkt *) malloc(n);
		} else {
			sync_rmap();
			continue;
		}

		n = pkt_rx((uint8_t *) pkt);

		if (n <= 0)
			printf("Error in pkt_rx()\n");


		gettimeofday(&t, NULL);

		/* time in ms since execute_op */
		elapsed_time  = (t.tv_sec  - t0.tv_sec)  * 1000.0;
		elapsed_time += (t.tv_usec - t0.tv_usec) / 1000.0;

		fee_pkt_hdr_to_cpu(pkt);

		if (pkt->hdr.type.last_pkt) {


			printf("last packet, seq %d, frame %d, data len %d type field %x\n", pkt->hdr.seq_cntr,
								pkt->hdr.frame_cntr,
								pkt->hdr.data_len,
								pkt->hdr.fee_pkt_type);

			if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_HK)
				printf("last packet was of type HK\n");
			if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_DATA)
				printf("last packet was of type DATA\n");
			if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_WMASK)
				printf("last packet was of type WMASK\n");
			if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_EV_DET)
				printf("last packet was of type EV_DET\n");


#if SYNC_SEL_TEST
			if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_EV_DET) {
				static int sync_cnt;
				sync_cnt++;
				if (sync_cnt == 3) {
					smile_fee_set_sync_sel(0);
					smile_fee_sync_sync_sel(DPU2FEE);
					sync_rmap();
				}
			}
#endif
		}

		if (!pkt->hdr.type.last_pkt && pkt->hdr.type.pkt_type == FEE_PKT_TYPE_HK) {
			printf("HK BUT WITHOUT LAST_PACKET!\n");
			continue;
		}



		if (fee_pkt_is_wandering_mask(pkt)) {

			printf("WMASK packet, seq %d, frame %d, data len %d type field %x\n", pkt->hdr.seq_cntr,
								pkt->hdr.frame_cntr,
								pkt->hdr.data_len,
								pkt->hdr.fee_pkt_type);



			/* wandering masks are identical to events, except for
			 * the packet type marker
			 */
			fee_pkt_wandering_mask_to_cpu(pkt);
			printf("WANDERING MASK!\n");
			fee_pkt_show_wandering_mask(pkt);

			continue; /* for EV_TEST_DIGITISE */
		}


		if (fee_pkt_is_event(pkt)) {
			fee_pkt_event_to_cpu(pkt);
			if (fee_event_is_xray(pkt, 5000, 150*8, 200)) {
				fwrite((void *)pkt, n, 1, fd);
				ev_cnt++;
#define SHOW_EVENTS 0
#if SHOW_EVENTS
				fee_pkt_show_event(pkt);
				printf("ev_cnt %d\n", ev_cnt);
#endif
			}
#if 1
#endif

			continue; /* for EV_TEST_DIGITISE */
		}
#if 0
		if (fee_ft_aggregate(ft, pkt) == 1)
			break;
#endif

	}
	fclose(fd);
	printf("->>> %d x-ray events classified\n", ev_cnt);

#ifdef SIM_DUMP_FITS
	save_fits("!E2.fits", ft->E2, ft->rows, ft->cols);
	save_fits("!E4.fits", ft->E4, ft->rows, ft->cols);
	save_fits("!F2.fits", ft->F2, ft->rows, ft->cols);
	save_fits("!F4.fits", ft->F4, ft->rows, ft->cols);
#endif

	fee_ft_aggregator_destroy(ft);

	free(pkt);
	printf("End test: EV detection sim\n");
}



/**
 * run tests
 */

static void smile_fee_run_tests(void)
{
//	smile_fee_test1();
//	smile_fee_test2();
//	smile_fee_test3();

	//smile_fee_test_ev_det_ft();
	smile_fee_test789();


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
