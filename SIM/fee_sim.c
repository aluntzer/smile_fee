/**
 * @file   fee_sim.c
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   2022
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @brief SMILE FEE simulator
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fee_sim.h>
#include <smile_fee.h>
#include <smile_fee_cfg.h>
#include <smile_fee_ctrl.h>
#include <smile_fee_rmap.h>

#include <byteorder.h>


static uint16_t *ccd2_e;
static uint16_t *ccd2_f;
static uint16_t *ccd4_e;
static uint16_t *ccd4_f;


struct fee_data_payload {
	struct fee_data_pkt *pkt;
	size_t data_len_max;
};



static void fee_sim_destroy_hk_data_payload(struct fee_hk_data_payload *hk)
{
	free(hk);
}

/**
 * @brief allocate and fill HK payload data
 *
 * @note the retured structure is either NULL or
 *	 sizeof(struct fee_hk_data_payload) long when valid
 */

static struct fee_hk_data_payload *fee_sim_create_hk_data_payload(void)
{
	struct fee_hk_data_payload *hk;


	hk = (struct fee_hk_data_payload * ) malloc(sizeof(struct fee_hk_data_payload));
	if (!hk) {
		perror("malloc");
		exit(-1);
	}

	/* XXX set actual HK data here once we get the contents/order info */

	return hk;
}


/**
 * @brief get a SpW-style time code, increments one tick for every call
 */

static uint8_t fee_get_spw_time_code(void)
{
	static uint8_t tc;

	return (tc++ & 0x3f);
}


/**
 * @brief set the logical destination address
 */

static void fee_sim_hdr_set_logical_addr(struct fee_data_hdr *hdr, uint8_t addr)
{
	hdr->logical_addr = addr;
}


/**
 * @brief set the protocol id
 */

static void fee_sim_hdr_set_protocol_id(struct fee_data_hdr *hdr, uint8_t id)
{
	hdr->proto_id = id;
}


/**
 * @brief set the packet type
 */

static void fee_sim_hdr_set_pkt_type(struct fee_data_hdr *hdr, uint8_t type)
{
	hdr->type.pkt_type = type & 0x3;
}

/**
 * @brief set the CCD side
 */

static void fee_sim_hdr_set_ccd_side(struct fee_data_hdr *hdr, uint8_t side)
{
	hdr->type.ccd_side = side & 0x3;
}

/**
 * @brief set the CCD id
 */

static void fee_sim_hdr_set_ccd_id(struct fee_data_hdr *hdr, uint8_t id)
{
	hdr->type.ccd_id = id & 0x1;
}

/**
 * @brief set the FEE mode
 */

static void fee_sim_hdr_set_fee_mode(struct fee_data_hdr *hdr, uint8_t mode)
{
	hdr->type.fee_mode = mode & 0xF;
}


/**
 * @brief set the last packet marker
 */

static void fee_sim_hdr_set_last_pkt(struct fee_data_hdr *hdr, uint8_t last)
{
	hdr->type.last_pkt = last & 0x1;
}


/**
 * @brief set the current frame counter
 */

static void fee_sim_hdr_set_frame_cntr(struct fee_data_hdr *hdr, uint16_t cntr)
{
	hdr->frame_cntr = cntr;
}


/**
 * @brief increment the sequence counter
 */

static void fee_sim_hdr_inc_seq_cntr(struct fee_data_hdr *hdr)
{
	hdr->seq_cntr++;
}

/**
 * @brief set the payload data size
 * @note a proper value is the user's responsibility
 */

static void fee_sim_hdr_set_data_len(struct fee_data_hdr *hdr, uint16_t len)
{
	hdr->data_len = len;
}


/**
 * @brief swap header bytes to correct endianess for transfer
 *
 * @note do this just before the transfer, swap back for manipulation
 *	 afterwards!
 */

static void fee_sim_hdr_cpu_to_tgt(struct fee_data_hdr *hdr)
{
	/* swap type field to correct byte order */
	hdr->data_len = __be16_to_cpu(hdr->data_len);
	hdr->fee_pkt_type = __be16_to_cpu(hdr->fee_pkt_type);
	hdr->frame_cntr   = __be16_to_cpu(hdr->frame_cntr);
	hdr->seq_cntr     = __be16_to_cpu(hdr->seq_cntr);
}

/**
 * @brief swap header bytes to correct endianess for use with local cpu
 *
 * @note before the transfer, swap back to target endianess!
 */

static void fee_sim_hdr_tgt_to_cpu(struct fee_data_hdr *hdr)
{
	/* swap type field to correct byte order */
	hdr->data_len	  = __cpu_to_be16(hdr->data_len);
	hdr->fee_pkt_type = __cpu_to_be16(hdr->fee_pkt_type);
	hdr->frame_cntr   = __cpu_to_be16(hdr->frame_cntr);
	hdr->seq_cntr     = __cpu_to_be16(hdr->seq_cntr);
}


/**
 * @brief destroy a payload data packet
 */

static void fee_sim_destroy_data_payload(struct fee_data_payload *pld)
{
	if (!pld)
		return;

	if (pld->pkt)
		free(pld->pkt);

	free(pld);
}


/**
 * @brief create a payload data packet according to the current FEE configuration
 */

static struct fee_data_payload *fee_sim_create_data_payload(void)
{
	size_t tx_size;
	size_t pkt_size_max;

	struct fee_data_payload *pld;



	tx_size = smile_fee_get_packet_size() - sizeof(struct fee_data_hdr);

	if (tx_size & 0x3) {
		printf("Warning, configured payload size must be a multiple of "
		       "4 according to SMILE-MSSL-PL-Register_map_v0.20, "
		       "clamping to next lower bound\n");

		tx_size &= ~0x3;
	}

	/* update and verify pkt_size_max */
	pkt_size_max =  sizeof(struct fee_data_hdr) + tx_size;
	if (pkt_size_max < (sizeof(struct fee_data_hdr) + 1)) {
		printf("Configured packet size must be at least header size "
		       "+ 1 or we won't be able to transfer anything\n");
		return NULL;
	}

	/* allocate payload */
	pld = (struct fee_data_payload * ) calloc(1, sizeof(struct fee_data_payload));
	if (!pld) {
		perror("malloc");
		exit(-1);
	}

	/* allocate the packet structure we use for the transfer of this frame */
	pld->pkt = (struct fee_data_pkt * ) calloc(1, pkt_size_max);
	if (!pld->pkt) {
		free(pld);
		perror("malloc");
		exit(-1);
	}

	pld->data_len_max = tx_size;

	return pld;
}


/**
 * @brief send a data packet
 */

static void fee_sim_send_data_payload(struct sim_net_cfg *cfg,
				      struct fee_data_payload *pld)
{
	size_t n;

	n = pld->pkt->hdr.data_len + sizeof(struct fee_data_pkt);

	/* send */
	fee_sim_hdr_cpu_to_tgt(&pld->pkt->hdr);
	fee_send_non_rmap(cfg, (uint8_t *) pld->pkt, n);
	fee_sim_hdr_tgt_to_cpu(&pld->pkt->hdr);
}



/**
 * @brief send buffer in chunks according to the configuration
 */

static void fee_sim_tx_payload_data(struct sim_net_cfg *cfg,
				      struct fee_data_payload *pld,
				      uint8_t *buf, size_t n)
{
	size_t i = 0;


	fee_sim_hdr_set_last_pkt(&pld->pkt->hdr, 0);

	fee_sim_hdr_set_data_len(&pld->pkt->hdr, pld->data_len_max);

	while (n) {

		if (n > pld->data_len_max) {
			memcpy(pld->pkt->data, &buf[i], pld->data_len_max);
			i += pld->data_len_max;
			n -= pld->data_len_max;
		} else {

			memcpy(pld->pkt->data, &buf[i], n);
			fee_sim_hdr_set_data_len(&pld->pkt->hdr, n);
			fee_sim_hdr_set_last_pkt(&pld->pkt->hdr, 1);
			n = 0;
		}
		/* send */
		fee_sim_send_data_payload(cfg, pld);
		fee_sim_hdr_inc_seq_cntr(&pld->pkt->hdr);
	}

}


/**
 * @brief execute a frame transfer block
 *
 * @param cfg	a struct sim_net_cfg;
 *
 * @param E2	E-side CCD2 buffer or NULL if unused
 * @param F2	F-side CCD2 buffer or NULL if unused
 * @param E3	E-side CCD2 buffer or NULL if unused
 * @param F4	F-side CCD4 buffer or NULL if unused
 *
 * @param n the lenght of the frame data buffer; this applies to all buffers
 *
 * this executes a nominal transfer sequence as per
 * MSSL-SMILE-SXI-IRD-0001 draft A0.14 Table 8-12
 *
 */

static void fee_sim_frame_transfer(struct sim_net_cfg *cfg,  uint8_t fee_mode,
				   uint8_t *E2, uint8_t *F2, uint8_t *E4, uint8_t *F4,
				   size_t n)
{
	struct fee_hk_data_payload *hk;
	struct fee_data_payload *pld;

	static uint16_t frame_cntr;


	switch (fee_mode) {
	case FEE_MODE_ID_FTP:
	case FEE_MODE_ID_FT:
		break;
	default:
		printf("Only FT type transfers are supported by this function\n");
		return;
	}

	pld = fee_sim_create_data_payload();

	/* required once */
	fee_sim_hdr_set_logical_addr(&pld->pkt->hdr, DPU_LOGICAL_ADDRESS);
	fee_sim_hdr_set_protocol_id(&pld->pkt->hdr, FEE_DATA_PROTOCOL);
	fee_sim_hdr_set_frame_cntr(&pld->pkt->hdr, frame_cntr);

	/* in FT mode, first packet in sequence is HK */
	hk = fee_sim_create_hk_data_payload();

	if (hk) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_HK);
		fee_sim_tx_payload_data(cfg, pld, (uint8_t *) hk,
					sizeof(struct fee_hk_data_payload));
		fee_sim_destroy_hk_data_payload(hk);
	}

	/* E2 next (if set) */
	if (E2) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_DATA);
		fee_sim_hdr_set_ccd_side(&pld->pkt->hdr, FEE_CCD_SIDE_E);
		fee_sim_hdr_set_ccd_id(&pld->pkt->hdr,   FEE_CCD_ID_2);
		fee_sim_hdr_set_fee_mode(&pld->pkt->hdr, fee_mode);
		fee_sim_tx_payload_data(cfg, pld, E2, n);
	}

	/* F2 next (if set) */
	if (F2) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_DATA);
		fee_sim_hdr_set_ccd_side(&pld->pkt->hdr, FEE_CCD_SIDE_F);
		fee_sim_hdr_set_ccd_id(&pld->pkt->hdr,   FEE_CCD_ID_2);
		fee_sim_hdr_set_fee_mode(&pld->pkt->hdr, fee_mode);
		fee_sim_tx_payload_data(cfg, pld, F2, n);
	}

	/* E4 next (if set) */
	if (E4) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_DATA);
		fee_sim_hdr_set_ccd_side(&pld->pkt->hdr, FEE_CCD_SIDE_E);
		fee_sim_hdr_set_ccd_id(&pld->pkt->hdr,   FEE_CCD_ID_4);
		fee_sim_hdr_set_fee_mode(&pld->pkt->hdr, fee_mode);
		fee_sim_tx_payload_data(cfg, pld, E4, n);
	}

	/* F4 next (if set) */
	if (F4) {
		fee_sim_hdr_set_pkt_type(&pld->pkt->hdr, FEE_PKT_TYPE_DATA);
		fee_sim_hdr_set_ccd_side(&pld->pkt->hdr, FEE_CCD_SIDE_F);
		fee_sim_hdr_set_ccd_id(&pld->pkt->hdr,   FEE_CCD_ID_4);
		fee_sim_hdr_set_fee_mode(&pld->pkt->hdr, fee_mode);
		fee_sim_tx_payload_data(cfg, pld, F4, n);
	}


	/* increment frame counter */
	frame_cntr++;

	fee_sim_destroy_data_payload(pld);
}


/**
 * @brief extract ccd data for frame transfer mode
 */

static uint16_t *fee_sim_get_ft_data(uint16_t *ccd, size_t rows, size_t cols,
				     size_t bins)
{
	size_t i, j;
	size_t x, y;
	size_t rw, cl;

	uint16_t *buf;
	uint16_t *acc;


	/* we need a cleared buffer to accumulate the bins */
	buf = calloc(sizeof(uint16_t), rows * cols);
	if (!buf) {
		perror("malloc");
		exit(-1);
	}

	if (bins == 1) {
		memcpy(buf, ccd, rows * cols * sizeof(uint16_t));
		return buf;
	}

	/* our accumulator */
	acc = malloc(FEE_CCD_IMG_SEC_COLS * sizeof(uint16_t));
	if (!acc) {
		perror("malloc");
		exit(-1);
	}


	/* the binned data contain overscan in the real FEE, i.e. the "edge"
	 * pixels contain CCD bias values, but we just ignore those
	 * and round down to the next integer
	 * note that we keep the nominal size, we just don't fill the
	 * out-of-bounds samples with values
	 */
	rw = FEE_CCD_IMG_SEC_ROWS / bins;
	cl = FEE_CCD_IMG_SEC_COLS / bins;


	/* rebinned rows */
	for (y = 0; y < rw; y++) {

		/* clear line accumulator */
		memset(acc, 0x0, FEE_CCD_IMG_SEC_COLS * sizeof(uint16_t));

		/* accumulate the data lines in "blocks" of bins */
		for (i = 0; i < bins; i++) {

			/* the offset into the buffer of the start of the
			 * next row; note that the original number of rows
			 * is required, otherwise the data would be skewed
			 */
			size_t y0 = y *  FEE_CCD_IMG_SEC_COLS + i * rows;

			for (j = 0; j < FEE_CCD_IMG_SEC_COLS; j++) {
				acc[j] += ccd[y0 + j];
			}

		}

		/* accumulate blocks of bins into columns */
		for (x = 0; x < cl; x++) {

			for (i = 0; i < bins; i++)
				buf[y * cols + x] += acc[x * bins + i];

		}

	}


	free(acc);

	return buf;
}


static void fee_sim_exec_ft_mode(struct sim_net_cfg *cfg)
{
	uint16_t *E2 = NULL;
	uint16_t *F2 = NULL;
	uint16_t *E4 = NULL;
	uint16_t *F4 = NULL;

	size_t rows, cols, bins;
	uint16_t readout;


	switch (smile_fee_get_ccd_mode2_config()) {

	case FEE_MODE2_NOBIN:
		rows = FEE_CCD_IMG_SEC_ROWS;
		cols = FEE_CCD_IMG_SEC_COLS;
		bins = 1;
		break;

	case FEE_MODE2_BIN6:
		rows = FEE_EDU_FRAME_6x6_ROWS;
		cols = FEE_EDU_FRAME_6x6_COLS;
		bins = 6;
		break;

	case FEE_MODE2_BIN24:
		/* values are guessed base on returned data from FEE, the
		 * actual size is not mentioned in the IRD
		 * it is unclear whether this mode is ever going to be used
		 */
		rows = FEE_EDU_FRAME_24x24_ROWS;
	        cols = FEE_EDU_FRAME_24x24_COLS;
		bins = 24;
		break;
	default:
		printf("Unknown binning mode specified\n");
		return;
	}

	readout = smile_fee_get_readout_node_sel();


	if (readout & FEE_READOUT_NODE_E2)
		E2 = fee_sim_get_ft_data(ccd2_e, rows, cols, bins);

	if (readout & FEE_READOUT_NODE_F2)
		F2 = fee_sim_get_ft_data(ccd2_f, rows, cols, bins);

	if (readout & FEE_READOUT_NODE_E4)
		E4 = fee_sim_get_ft_data(ccd4_e, rows, cols, bins);

	if (readout & FEE_READOUT_NODE_F4)
		F4 = fee_sim_get_ft_data(ccd4_f, rows, cols, bins);

	fee_sim_frame_transfer(cfg, FEE_MODE_ID_FTP,
			       (uint8_t *) E2, (uint8_t *) F2,
			       (uint8_t *) E4, (uint8_t *) F4,
			       sizeof(uint16_t) * rows * cols);

	free(E2);
	free(F2);
	free(E4);
	free(F4);
}



/**
 * @brief generate a pattern for frame transfer pattern mode
 */

static struct fee_pattern *fee_sim_gen_ft_pat(uint8_t ccd_side, uint8_t ccd_id,
					      size_t rows, size_t cols)
{
	size_t i, j;

	struct fee_pattern pix;
	struct fee_pattern *buf;


	buf = malloc(sizeof(struct fee_pattern) * rows * cols);
	if (!buf) {
		perror("malloc");
		exit(-1);
	}

	pix.side = ccd_side;
	pix.ccd  = ccd_id;
	pix.time_code = fee_get_spw_time_code() & 0x7;

	for (i = 0; i < rows; i++) {

		pix.row = i & 0x1F;

		for (j = 0; j < cols; j++) {

			pix.col = j & 0x1F;

#if 0
			buf[i * cols + j].field = __cpu_to_be16(pix.field);
#else
			buf[i * cols + j].field = pix.field;
#endif
		}
	}

	return buf;
}



static void fee_sim_exec_ft_pat_mode(struct sim_net_cfg *cfg)
{
	struct fee_pattern *E2 = NULL;
	struct fee_pattern *F2 = NULL;
	struct fee_pattern *E4 = NULL;
	struct fee_pattern *F4 = NULL;

	size_t rows, cols;
	uint16_t readout;


	switch (smile_fee_get_ccd_mode2_config()) {

	case FEE_MODE2_NOBIN:
		rows = FEE_CCD_IMG_SEC_ROWS;
		cols = FEE_CCD_IMG_SEC_COLS;
		break;

	case FEE_MODE2_BIN6:
		rows = FEE_EDU_FRAME_6x6_ROWS;
		cols = FEE_EDU_FRAME_6x6_COLS;
		break;

	case FEE_MODE2_BIN24:
		/* values are guessed base on returned data from FEE, the
		 * actual size is not mentioned in the IRD
		 * it is unclear whether this mode is ever going to be used
		 */
		rows = FEE_EDU_FRAME_24x24_ROWS;
		cols = FEE_EDU_FRAME_24x24_COLS;
		break;
	default:
		printf("Unknown binning mode specified\n");
		return;
	}

	readout = smile_fee_get_readout_node_sel();


	if (readout & FEE_READOUT_NODE_E2)
		E2 = fee_sim_gen_ft_pat(FEE_CCD_SIDE_E, FEE_CCD_ID_2, rows, cols);

	if (readout & FEE_READOUT_NODE_F2)
		F2 = fee_sim_gen_ft_pat(FEE_CCD_SIDE_F, FEE_CCD_ID_2, rows, cols);

	if (readout & FEE_READOUT_NODE_E4)
		E4 = fee_sim_gen_ft_pat(FEE_CCD_SIDE_E, FEE_CCD_ID_4, rows, cols);

	if (readout & FEE_READOUT_NODE_F4)
		F4 = fee_sim_gen_ft_pat(FEE_CCD_SIDE_F, FEE_CCD_ID_4, rows, cols);


	fee_sim_frame_transfer(cfg, FEE_MODE_ID_FTP,
			       (uint8_t *) E2, (uint8_t *) F2,
			       (uint8_t *) E4, (uint8_t *) F4,
			       sizeof(struct fee_pattern) * rows * cols);

	free(E2);
	free(F2);
	free(E4);
	free(F4);
}


/**
 * @brief main simulator execution
 */

static void fee_sim_exec(struct sim_net_cfg *cfg)
{
	uint8_t mode;


	mode = smile_fee_get_ccd_mode_config();

	switch (mode) {

	case FEE_MODE_ID_ON:
		/* the thing is switched on */
		printf("We're switched on, cool!\n");
		break;
	case FEE_MODE_ID_FTP:
		/* frame transfer pattern */
		printf("Frame Transfer Pattern Mode\n");
		fee_sim_exec_ft_pat_mode(cfg);
		break;
	case FEE_MODE_ID_STBY:
		/* stand-by-mode */
		printf("We're in stand-by, no idea what that does\n");
		break;
	case FEE_MODE_ID_FT:
		/* frame transfer */
		printf("Frame Transfer Mode\n");
		fee_sim_exec_ft_mode(cfg);
		break;
	case FEE_MODE_ID_FF:
		/* full frame */
		printf("Mode %d not implemented\n", mode);
		break;
	case FEE_CMD__ID_IMM_ON:
		/* immediate on-mode, this is a command, not a mode */
		printf("Mode %d not implemented\n", mode);
		break;
	case FEE_MODE_ID_PTP1:
		/* parallel trap pump mode 1 */
		printf("Mode %d not implemented\n", mode);
		break;
	case FEE_MODE_ID_PTP2:
		/* parallel trap pump mode 2 */
		printf("Mode %d not implemented\n", mode);
		break;
	case FEE_MODE_ID_STP1:
		/* serial trap pump mode 1 */
		printf("Mode %d not implemented\n", mode);
		break;
	case FEE_MODE_ID_STP2:
		/* serial trap pump mode 2 */
		printf("Mode %d not implemented\n", mode);
		break;
	default:
		printf("Unknown mode %d, ignoring\n", mode);
		break;
	}
}


/**
 * @brief sim main loop
 */

void fee_sim_main(struct sim_net_cfg *cfg)
{
	/* we have 2 ccds, each with 2 sides (E&F) */
	const size_t img_side_pix = FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS;

	ccd2_e = (uint16_t *) malloc(img_side_pix * sizeof(uint16_t));
	if (!ccd2_e) {
		perror("malloc");
		exit(-1);
	}

	ccd2_f = (uint16_t *) malloc(img_side_pix * sizeof(uint16_t));
	if (!ccd2_f) {
		perror("malloc");
		exit(-1);
	}

	ccd4_e = (uint16_t *) malloc(img_side_pix * sizeof(uint16_t));
	if (!ccd4_e) {
		perror("malloc");
		exit(-1);
	}

	ccd4_f = (uint16_t *) malloc(img_side_pix * sizeof(uint16_t));
	if (!ccd4_f) {
		perror("malloc");
		exit(-1);
	}

	/* simulator main loop */
	while (1) {

		if (!smile_fee_get_execute_op()) {
			/* poll every 1 ms */
			usleep(1000);
			continue;
		}

		printf("EXECUTE OP!\n");
		fee_sim_exec(cfg);

		smile_fee_set_execute_op(0);
		printf("OP complete\n");
	}

	free(ccd2_e);
	free(ccd2_f);
	free(ccd4_e);
	free(ccd4_f);
}
