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



#include <debug.h>
#include <string.h>
#include <stdlib.h>

#include <smile_fee.h>
#include <smile_fee_ctrl.h>

#include <byteorder.h>


#ifndef __WORDSIZE
#define __WORDSIZE (__SIZEOF_LONG__ * 8)
#endif

#ifndef BITS_PER_LONG
#define BITS_PER_LONG __WORDSIZE
#endif

#define BIT_WORD(nr)            ((nr) / BITS_PER_LONG)


static uint32_t *fee_ccd2_bad_pixels;
static uint32_t *fee_ccd4_bad_pixels;


/**
 * test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 */
static inline int test_bit(int nr, const volatile uint32_t *addr)
{
        return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
}


static void fee_pkt_event_to_cpu_internal(struct fee_data_pkt *pkt)
{
	size_t i;
	struct fee_event_detection *ev;

	ev = (struct fee_event_detection *) pkt;

	ev->row = be16_to_cpu(ev->row);
	ev->col = be16_to_cpu(ev->col);

	for (i = 0; i < FEE_EV_DET_PIXELS; i++)
		ev->pix[i] = be16_to_cpu(ev->pix[i]);
}


/**
 * @brief show the event packet contents
 */

static void fee_pkt_show_event_internal(struct fee_data_pkt *pkt)
{
	struct fee_event_detection *ev;


	ev = (struct fee_event_detection *) pkt;


	if (pkt->hdr.type.ccd_id == FEE_CCD_ID_2)
		DBG("CDD 2 ");
	else if (pkt->hdr.type.ccd_id == FEE_CCD_ID_4)
		DBG("CDD 4 ");

	if (pkt->hdr.type.ccd_side == FEE_CCD_SIDE_E)
		DBG("Side E ");
	else if (pkt->hdr.type.ccd_side == FEE_CCD_SIDE_F)
		DBG("Side F ");

	DBG("at row %d col %d, value %d\n", ev->row, ev->col, ev->pix[FEE_EV_PIXEL_IDX]);
}



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
 * @brief check if this is an wandering mask packet
 *
 * @returns 0 if not an wandering mask packet, bits set otherwise
 */

int fee_pkt_is_wandering_mask(struct fee_data_pkt *pkt)
{
	if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_WMASK)
		return 1;

	return 0;
}


/**
 * @brief show the event info in an event packet
 *
 */

void fee_pkt_show_event(struct fee_data_pkt *pkt)
{
	if (!fee_pkt_is_event(pkt))
		return;

	DBG("Event in ");
	fee_pkt_show_event_internal(pkt);
}


/**
 * @brief show the wandering mask info
 *
 * @note wandering masks use the same data structure as events
 *
 */

void fee_pkt_show_wandering_mask(struct fee_data_pkt *pkt)
{
	if (!fee_pkt_is_wandering_mask(pkt))
		return;

	DBG("Wandering mask of ");
	fee_pkt_show_event_internal(pkt);
}



/**
 * @brief swap the data endianess in an event packet
 *
 */

void fee_pkt_event_to_cpu(struct fee_data_pkt *pkt)
{
	if (!fee_pkt_is_event(pkt))
		return;

	fee_pkt_event_to_cpu_internal(pkt);
}

/**
 * @brief swap the data endianess in an event packet
 *
 */

void fee_pkt_wandering_mask_to_cpu(struct fee_data_pkt *pkt)
{
	if (!fee_pkt_is_wandering_mask(pkt))
		return;

	fee_pkt_event_to_cpu_internal(pkt);
}





/**
 * @brief check pixel mask for pixels marked invalid
 * #
 * @returns 1 if the event is marked as bad or is non-existant; 0 otherwise
 *
 * @note this is for use with event packets
 *
 * @note there are some extra sanity checks which can be disabled in flight
 * of we need to squeeze out a few more cycles...
 *
 */

int fee_event_pixel_is_bad(struct fee_data_pkt *pkt)
{
	size_t idx;

	uint32_t *tbl;

	struct fee_event_detection *ev;


	if (!fee_pkt_is_event(pkt))
		return 1;


	ev = (struct fee_event_detection *) pkt;

	idx = ev->row * FEE_EDU_FRAME_6x6_COLS + ev->col;

	if (idx > FEE_EDU_FRAME_6x6_COLS * FEE_EDU_FRAME_6x6_ROWS)
		return 1;

	if (pkt->hdr.type.ccd_id == FEE_CCD_ID_2)
		tbl = fee_ccd2_bad_pixels;

	if (pkt->hdr.type.ccd_id == FEE_CCD_ID_4)
		tbl = fee_ccd4_bad_pixels;

	if (!tbl)
		return 1;

	return test_bit(idx, tbl);
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
		DBG("Event pixel over threshold, not an x-ray\n");
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
		DBG("Ring sum (%d) over threshold, not an x-ray\n", sum);
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
		DBG("Too many ring pixels (%d) over threshold, not an x-ray\n", cnt);
		fee_pkt_show_event(pkt);
#endif
		return 0;
	}



	return 1;
}


/**
 * @brief destroy a FF data aggregator structure
 */

void fee_ff_aggregator_destroy(struct fee_ff_data *ff)
{
	if (!ff)
		return;

	free(ff->data);
	free(ff);
}


/**
 * @brief create a FF data aggregator structure
 *
 * @returns NULL on error, pointer otherwise
 *
 * @warn make sure the FEE/DPU register mirror is synced before calling this
 *	 function
 */

struct fee_ff_data *fee_ff_aggregator_create(void)
{
	struct fee_ff_data *ff;


	/* we need the pointers and counters to be cleared */
	ff = (struct fee_ff_data *) calloc(sizeof(struct fee_ff_data), 1);
	if (!ff) {
		DBG("Could not allocate fee data container");
		return NULL;
	}

	ff->n_elem  = FEE_CCD_IMG_SEC_ROWS * FEE_CCD_IMG_SEC_COLS;

	ff->data = (uint16_t *) malloc(sizeof(uint16_t) * ff->n_elem);
	if (!ff->data) {
		DBG("Could not allocate data buffer");
		free(ff);
		return NULL;
	}


	/* FF modes read only one CCD at a time
	 * as per reg map v0.22, the FEE interprets any value in the
	 * register != 1 as CCD4, otherwise CCD2
	 * I guess this is so a readout is guaranteed when starting the mode
	 */
	if (smile_fee_get_ccd_readout(1))
		ff->ccd_id = FEE_CCD_ID_2;
	else
		ff->ccd_id = FEE_CCD_ID_4;


	return ff;
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

int fee_ff_aggregate(struct fee_ff_data *ff, struct fee_data_pkt *pkt)
{
	int ret = 0;
	ssize_t n_elem;


	if (!ff)
		goto error;

	if (!pkt)
		goto error;



	if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_HK) {
		/* XXX HK is currently incomplete; this must be fixed in the
		 * FEE HW; we copy the data as long it does not exceed
		 * our allocate size
		 */
		if (pkt->hdr.data_len <= FEE_HK_PACKET_DATA_LEN) {
			memcpy(&ff->hk, &pkt->data, pkt->hdr.data_len);
		} else {
			DBG("HK packet is oversized!\n");
			goto error;
		}
	}

	if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_DATA) {

		n_elem = (ssize_t) pkt->hdr.data_len / sizeof(uint16_t);

		if ((ssize_t) ff->n - n_elem < (ssize_t) ff->n_elem) {
			memcpy(&ff->data[ff->n], &pkt->data, pkt->hdr.data_len);
			ff->n += n_elem;
		} else {
			printf("FF data oversized! %d vs %d \n", ff->n - n_elem, ff->n_elem);
			goto error;
		}

		/* only last packet marker in last packet marks last packet in
		 * frame ;)
		 */
		if (pkt->hdr.type.last_pkt)
			ret = 1;
	}

	return ret;

error:
	return -1;
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
		DBG("Unknown binning mode, cannot continue\n");
		return NULL;
	}


	/* we need the pointers and counters to be cleared */
	ft = (struct fee_ft_data *) calloc(sizeof(struct fee_ft_data), 1);
	if (!ft) {
		DBG("Could not allocate fee data container");
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
		DBG("Could not allocate data buffer");
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
				DBG("E2 data oversized!\n");
				return -1;
			}
		}

		if (pkt->hdr.type.ccd_id == FEE_CCD_ID_4) {
			if ((ssize_t) ft->n_E4  - n_elem < (ssize_t) ft->n_elem) {
				memcpy(&ft->E4[ft->n_E4], &pkt->data, pkt->hdr.data_len);
				ft->n_E4 += n_elem;
			} else {
				DBG("E4 data oversized!\n");
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
				DBG("F2 data oversized!\n");
				return -1;
			}
		}

		if (pkt->hdr.type.ccd_id == FEE_CCD_ID_4) {
			if ((ssize_t) ft->n_F4  - n_elem < (ssize_t) ft->n_elem) {
				memcpy(&ft->F4[ft->n_F4], &pkt->data, pkt->hdr.data_len);
				ft->n_F4 += n_elem;
			} else {
				DBG("F4 data oversized!\n");
				return -1;
			}
		}
	}

	return 0;
}



static int fee_ft_frame_complete(struct fee_ft_data *ft, struct fee_data_pkt *pkt)
{
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
		goto error;

	if (!pkt)
		goto error;

	if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_DATA) {

		ret = fee_ft_aggregate_assign_data(ft,pkt);

	} else if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_HK) {
		/* XXX HK is currently incomplete; this must be fixed in the
		 * FEE HW; we copy the data as long it does not exceed
		 * our allocate size
		 */
		if (pkt->hdr.data_len <= FEE_HK_PACKET_DATA_LEN) {
			memcpy(&ft->hk, &pkt->data, pkt->hdr.data_len);
		} else {
			DBG("HK packet is oversized!\n");
			goto error;
		}

	} else if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_EV_DET) {
		ret = 0;	/* don't care */
	} else if (pkt->hdr.type.pkt_type == FEE_PKT_TYPE_WMASK) {
		ret = 0;	/* don't care */
	} else {
		DBG("Unknown pkt type %d\n", pkt->hdr.fee_pkt_type);
		goto error;
	}

	if (!ret)
		ret = fee_ft_frame_complete(ft, pkt);


	return ret;

error:
	return -1;
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

	DBG("\n\tCOL %d ROW %d\n", pkt->col, pkt->row);

	/* as per MSSL-SMILE-SXI-IRD-0001, req. MSSL-IF-91  tbl 8-11,
	 * the upper left pixel is the last pixel in the data
	 * and the lower left pixel is the first pixel in the data
	 */
	for (i = FEE_EV_ROWS - 1; i >= 0; i--) {

		DBG("\t");

		for (j = 0; j <  FEE_EV_COLS; j++)
			DBG("%05d ", pkt->pix[j + i * FEE_EV_COLS]);

		DBG("\n");
	}

	DBG("\n");
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



