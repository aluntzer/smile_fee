/**
 * @file   smile_fee.c
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   2020
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
 * @brief SMILE FEE interface
 *
 * @warn byte order is BIG ENDIAN!
 *
 */



#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <smile_fee.h>
#include <smile_fee_ctrl.h>

#include <byteorder.h>



/**
 * @brief in-place swap header fields to architecture endianess
 */

void fee_pkt_hdr_to_cpu(struct fee_data_pkt *pkt)
{
	pkt->hdr.data_len	= __be16_to_cpu(pkt->hdr.data_len);
	pkt->hdr.fee_pkt_type   = __be16_to_cpu(pkt->hdr.fee_pkt_type);
	pkt->hdr.frame_cntr	= __be16_to_cpu(pkt->hdr.frame_cntr);
	pkt->hdr.seq_cntr	= __be16_to_cpu(pkt->hdr.seq_cntr);
}


/**
 * @brief check if this is an event packet
 *
 * @returns 0 if not an event packet, bits set otherwise
 */

int fee_pkt_is_event(struct fee_data_pkt *pkt)
{
	if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_EV_DET)
		return 1;

	return 0;
}



/**
 * @brief show the event info in an event packet
 *
 */

void fee_pkt_show_event(struct fee_data_pkt *pkt)
{
	struct fee_event_detection *ev;


	if (!fee_pkt_is_event(pkt))
		return;


	ev = (struct fee_event_detection *) pkt;

	printf("Event in ");

	if (pkt->hdr.type.ccd_id == FEE_CCD_ID_2)
		printf("CDD 2 ");
	else if (pkt->hdr.type.ccd_id == FEE_CCD_ID_4)
		printf("CDD 4 ");

	if (pkt->hdr.type.ccd_side == FEE_CCD_SIDE_E)
		printf("Side E ");
	else if (pkt->hdr.type.ccd_side == FEE_CCD_SIDE_F)
		printf("Side F ");

	printf("at row %d col %d, value %d\n", ev->row, ev->col, ev->pix[FEE_EV_PIXEL_IDX]);
}



/**
 * @brief swap the data endianess in an event packet
 *
 */

void fee_pkt_event_to_cpu(struct fee_data_pkt *pkt)
{
	size_t i;
	struct fee_event_detection *ev;


	if (!fee_pkt_is_event(pkt))
		return;


	ev = (struct fee_event_detection *) pkt;

	ev->row = be16_to_cpu(ev->row);
	ev->col = be16_to_cpu(ev->col);

	for (i = 0; i < FEE_EV_DET_PIXELS; i++)
		ev->pix[i] = be16_to_cpu(ev->pix[i]);

}


/**
 * @brief perform event classification
 *
 *
 * @param centre_th the threshold above which the centre pixel is considered non-X-ray
 * @param sum_th    the threshold above which the sum of the ring around centre is considered non-X-ray
 * @param ring_th   the threshold of singled pixels for inclusion in event classification
 *
 * see TN "SMILE SXI CCD Testing and Calibration
 * Event Detection Methodology" issue 2 rev 0, section "Data Sorting Algorithm"
 *
 * @returns 1 if the event is considered an X-ray, 0 otherwise
 *
 */

int fee_event_is_xray(struct fee_data_pkt *pkt,
		      uint16_t centre_th, uint32_t sum_th, uint16_t ring_th)
{
#define PIXEL_RING_COUNT_MAX 4

	int cnt = 0;
	uint32_t sum = 0;
	struct fee_event_detection *ev;


	if (!fee_pkt_is_event(pkt))
		return 0;

	ev = (struct fee_event_detection *) pkt;

	if (ev->pix[FEE_EV_PIXEL_IDX] > centre_th) {
#if 0
		printf("Event pixel over threshold, not an x-ray\n");
		fee_pkt_show_event(pkt);
#endif
		return 0;
	}

	sum += ev->pix[6];
	sum += ev->pix[7];
	sum += ev->pix[8];
	sum += ev->pix[11];
	sum += ev->pix[13];
	sum += ev->pix[16];
	sum += ev->pix[17];
	sum += ev->pix[18];

	if (sum > sum_th) {
#if 0
		printf("Ring sum (%d) over threshold, not an x-ray\n", sum);
		fee_pkt_show_event(pkt);
#endif
		return 0;
	}

	if (ev->pix[6] > ring_th)
		cnt++;
	if (ev->pix[7] > ring_th)
		cnt++;
	if (ev->pix[8] > ring_th)
		cnt++;
	if (ev->pix[11] > ring_th)
		cnt++;
	if (ev->pix[13] > ring_th)
		cnt++;
	if (ev->pix[16] > ring_th)
		cnt++;
	if (ev->pix[17] > ring_th)
		cnt++;
	if (ev->pix[18] > ring_th)
		cnt++;

	if (cnt > PIXEL_RING_COUNT_MAX) {
#if 0
		printf("Too many ring pixels (%d) over threshold, not an x-ray\n", cnt);
		fee_pkt_show_event(pkt);
#endif
		return 0;
	}



	return 1;
}





/**
 * @brief destroy a FT data aggregator structure
 */

void fee_ft_aggregator_destroy(struct fee_ft_data *ft)
{
	if (!ft)
		return;

	free(ft->data);
	free(ft);
}


/**
 * @brief create a FT data aggregator structure
 *
 * @returns NULL on error, pointer otherwise
 *
 * @warn make sure the FEE/DPU register mirror is synced before calling this
 *	 function
 */

struct fee_ft_data *fee_ft_aggregator_create(void)
{
	size_t rows;
	size_t cols;
	size_t bins;
	size_t nodes;
	size_t off;

	struct fee_ft_data *ft;


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
		rows = FEE_EDU_FRAME_24x24_ROWS;
		cols = FEE_EDU_FRAME_24x24_COLS;
		bins = 24;
		break;
	default:
		printf("Unknown binning mode, cannot continue\n");
		return NULL;
	}


	/* we need the pointers and counters to be cleared */
	ft = (struct fee_ft_data *) calloc(sizeof(struct fee_ft_data), 1);
	if (!ft) {
		printf("Could not allocate fee data container");
		return NULL;
	}

	ft->rows    = rows;
	ft->cols    = cols;
	ft->bins    = bins;
	ft->n_elem  = rows * cols;
	ft->readout = smile_fee_get_readout_node_sel();

	/* allocate one frame size per readout node, readout is a bitmask */
	nodes = __builtin_popcount(ft->readout);

	ft->data = (uint16_t *) malloc(sizeof(uint16_t) * ft->n_elem * nodes);
	if (!ft->data) {
		printf("Could not allocate data buffer");
		free(ft);
		return NULL;
	}

	off = 0;
	/* set the pointers */
	if (ft->readout & FEE_READOUT_NODE_E2) {
		ft->E2 = &ft->data[off];
		off += ft->n_elem;
	}

	if (ft->readout & FEE_READOUT_NODE_F2) {
		ft->F2 = &ft->data[off];
		off += ft->n_elem;
	}

	if (ft->readout & FEE_READOUT_NODE_E4) {
		ft->E4 = &ft->data[off];
		off += ft->n_elem;
	}

	if (ft->readout & FEE_READOUT_NODE_F4) {
		ft->F4 = &ft->data[off];
		off += ft->n_elem;
	}


	return ft;
}


static int fee_ft_aggregate_assign_data(struct fee_ft_data *ft, struct fee_data_pkt *pkt)
{

	ssize_t n_elem = (ssize_t) pkt->hdr.data_len / sizeof(uint16_t);


	if (pkt->hdr.type.ccd_side == FEE_CCD_SIDE_E) {

		if (pkt->hdr.type.ccd_id == FEE_CCD_ID_2) {
			if ((ssize_t) ft->n_E2  - n_elem < (ssize_t) ft->n_elem) {
				memcpy(&ft->E2[ft->n_E2], &pkt->data, pkt->hdr.data_len);
				ft->n_E2 += n_elem;
			} else {
				printf("E2 data oversized!\n");
				exit(-1);
				return -1;
			}
		}

		if (pkt->hdr.type.ccd_id == FEE_CCD_ID_4) {
			if ((ssize_t) ft->n_E4  - n_elem < (ssize_t) ft->n_elem) {
				memcpy(&ft->E4[ft->n_E4], &pkt->data, pkt->hdr.data_len);
				ft->n_E4 += n_elem;
			} else {
				printf("E4 data oversized!\n");
				return -1;
			}
		}
	}


	if (pkt->hdr.type.ccd_side == FEE_CCD_SIDE_F) {

		if (pkt->hdr.type.ccd_id == FEE_CCD_ID_2) {
			if ((ssize_t) ft->n_F2  - n_elem < (ssize_t) ft->n_elem) {
				memcpy(&ft->F2[ft->n_F2], &pkt->data, pkt->hdr.data_len);
				ft->n_F2 += n_elem;
			} else {
				printf("F2 data oversized!\n");
				return -1;
			}
		}

		if (pkt->hdr.type.ccd_id == FEE_CCD_ID_4) {
			if ((ssize_t) ft->n_F4  - n_elem < (ssize_t) ft->n_elem) {
				memcpy(&ft->F4[ft->n_F4], &pkt->data, pkt->hdr.data_len);
				ft->n_F4 += n_elem;
			} else {
				printf("F4 data oversized!\n");
				return -1;
			}
		}
	}


	/* clear the side bit in the readout field on last packet marker
	 * until none remain
	 */
	if (pkt->hdr.type.last_pkt) {
		if (pkt->hdr.type.ccd_side == FEE_CCD_SIDE_E) {
			if (pkt->hdr.type.ccd_id == FEE_CCD_ID_2)
				ft->readout &= ~FEE_READOUT_NODE_E2;
			if (pkt->hdr.type.ccd_id == FEE_CCD_ID_4)
				ft->readout &= ~FEE_READOUT_NODE_E4;
		}

		if (pkt->hdr.type.ccd_side == FEE_CCD_SIDE_F) {
			if (pkt->hdr.type.ccd_id == FEE_CCD_ID_2)
				ft->readout &= ~FEE_READOUT_NODE_F2;
			if (pkt->hdr.type.ccd_id == FEE_CCD_ID_4)
				ft->readout &= ~FEE_READOUT_NODE_F4;
		}
	}

	if (ft->readout)
		return 0;

	/* none remain */
	return 1;
}


/**
 * @brief frame data aggeregator
 *
 * returns 0 if frame is incomplete
 *	   1 if last_pkt was set (== data frame ready)
 *	  -1 on error
 *
 * @warn this function requires all packet header values to be in correct
 *	 endianess for the architecture, i.e. use fee_pkt_hdr_to_cpu() first
 */

int fee_ft_aggregate(struct fee_ft_data *ft, struct fee_data_pkt *pkt)
{
	int ret = 0;


	if (!ft)
		return -1;

	if (!pkt)
		return -1;

	if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_DATA) {

		ret = fee_ft_aggregate_assign_data(ft,pkt);

	} else if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_HK) {
		/* HK is currently incomplete; this must be fixed in the
		 * FEE HW; we copy the data as long it does not exceed
		 * our allocate size
		 */
		if (pkt->hdr.data_len <= FEE_HK_PACKET_DATA_LEN) {
			memcpy(&ft->hk, &pkt->data, pkt->hdr.data_len);
		} else {
			printf("HK packet is oversized!\n");
			ret = -1;
		}
	} else if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_EV_DET) {
		ret = 0;	/* don't care */
	} else {
		printf("Unknown pkt type %d\n", pkt->hdr.fee_pkt_type);
		ret = -1;
	}


	return ret;
}



/**
 * @brief write the contents of an event package to the console
 *
 * @param pkt a struct fee_display_event
 */

void fee_display_event(const struct fee_event_detection *pkt)
{
	ssize_t i, j;


	if (!pkt)
		return;

	printf("\n\tCOL %d ROW %d\n", pkt->col, pkt->row);

	/* as per MSSL-SMILE-SXI-IRD-0001, req. MSSL-IF-91  tbl 8-11,
	 * the upper left pixel is the last pixel in the data
	 * and the lower left pixel is the first pixel in the data
	 */
	for (i = FEE_EV_ROWS - 1; i >= 0; i--) {

		printf("\t");

		for (j = 0; j <  FEE_EV_COLS; j++)
			printf("%05d ", pkt->pix[j + i * FEE_EV_COLS]);

		printf("\n");
	}

	printf("\n");
}


void test_fee_display_event(void)
{
	ssize_t i, j;
	struct fee_event_detection pkt;


	pkt.col = 12;
	pkt.row = 43;

	for (i = FEE_EV_ROWS - 1; i >= 0; i--)
		for (j = 0; j <  FEE_EV_COLS; j++)
			pkt.pix[j + i * FEE_EV_COLS] = j + i * FEE_EV_COLS;

	fee_display_event(&pkt);
}

#if 0
int main(void)
{
	test_fee_display_event();
}
#endif



