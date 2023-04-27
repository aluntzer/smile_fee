/**
 * @file   smile_fee.h
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
 */

#ifndef SMILE_FEE_H
#define SMILE_FEE_H

#include <stdint.h>



/**
 *  @see MSSL-SMILE-SXI-IRD-0001 Draft A.14
 *
 *  @warn be aware that requirement numbering can randomly change between
 *	  versions of the IRD, they appear to be a generated sequence
 *	  rather than explicitly entered identifiers
 */

#define DPU_LOGICAL_ADDRESS	0x50
#define FEE_LOGICAL_ADDRESS	0x51

#define RMAP_PROTOCOL_ID	0x01
#define FEE_DATA_PROTOCOL	0xF0	/* Draft A.14,  MSSL-IF-106 */



/**
 * FEE modes
 *
 * @see MSSL-SMILE-SXI-IRD-0001  Draft A.14, req. MSSL-IF-17
 *	also SMILE-MSSL-PL-Register_map_v0.22, as the IRD does not list all
 *	modes
 */


#define FEE_MODE_ID_ON		0x0	/* the thing is switched on */
#define FEE_MODE_ID_FTP		0x1	/* frame transfer pattern */
#define FEE_MODE_ID_STBY	0x2	/* stand-by-mode */
#define FEE_MODE_ID_FT		0x3	/* frame transfer */
#define FEE_MODE_ID_FF		0x4	/* full frame */
#define FEE_CMD__ID_SOFT_RST	0x7	/* soft reset, (not specified, but likely a command?) */
#define FEE_CMD__ID_IMM_ON	0x8	/* immediate on-mode, this is a command, not a mode */
#define FEE_MODE_ID_FFSIM	0x9	/* full frame simulation simulation */
#define FEE_MODE_ID_EVSIM	0xA	/* event detection simulation */
#define FEE_MODE_ID_PTP1	0xB	/* parallel trap pump mode 1 */
#define FEE_MODE_ID_PTP2	0xC	/* parallel trap pump mode 2 */
#define FEE_MODE_ID_STP1	0xD	/* serial trap pump mode 1 */
#define FEE_MODE_ID_STP2	0xE	/* serial trap pump mode 2 */

#define FEE_MODE2_NOBIN		0x1	/* no binning mode */
#define FEE_MODE2_BIN6		0x2	/* 6x6 binning mode */
#define FEE_MODE2_BIN24		0x3	/* 24x4 binning mode */

/* these identifiy the bits in the readout node selection register */
#define FEE_READOUT_NODE_E2	0x2	/* 0b0010 */
#define FEE_READOUT_NODE_F2	0x1	/* 0b0001 */
#define FEE_READOUT_NODE_E4	0x8	/* 0b1000 */
#define FEE_READOUT_NODE_F4	0x4	/* 0b0100 */


/* @see MSSL-SMILE-SXI-IRD-0001 Draft A.14, req. MSSL-IF-108 */

#define FEE_CCD_SIDE_F	0x0		/* left side */
#define FEE_CCD_SIDE_E	0x1		/* right side */
#define FEE_CCD_INTERLEAVED	0x2	/* F and E inverleaved */

#define FEE_CCD_ID_2		0x0
#define FEE_CCD_ID_4		0x1

#define FEE_PKT_TYPE_DATA	0x0	/* any data */
#define FEE_PKT_TYPE_EV_DET	0x1	/* event detection */
#define FEE_PKT_TYPE_HK		0x2	/* housekeeping */
#define FEE_PKT_TYPE_WMASK	0x3	/* wandering mask packet */



/* @see MSSL-SMILE-SXI-IRD-0001 Draft A.14, req. MSSL-IF-108 */
__extension__
struct fee_pkt_type {
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	uint16_t reserved0:4;
	uint16_t fee_mode:4;
	uint16_t last_pkt:1;	/* if set: last packet in readout cycle */
	uint16_t ccd_side:2;
	uint16_t ccd_id:1;
	uint16_t reserved1:2;
	uint16_t pkt_type:2;
#elif (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	uint16_t pkt_type:2;
	uint16_t reserved1:2;
	uint16_t ccd_id:1;
	uint16_t ccd_side:2;
	uint16_t last_pkt:1;	/* if set: last packet in readout cycle */
	uint16_t fee_mode:4;
	uint16_t reserved0:4;
#endif
} __attribute__((packed));

/* XXX need proper include file in path for compile time size checks */
#if 0
compile_time_assert((sizeof(struct fee_data_hdr)
                     == sizeof(uint16_t)),
                    FEE_PKT_TYPE_STRUCT_WRONG_SIZE);
#endif


/* @see MSSL-SMILE-SXI-IRD-0001 Draft A.14, req. MSSL-IF-103, MSSL-IF-108 */
__extension__
struct fee_data_hdr {
	uint8_t logical_addr;
	uint8_t proto_id;
	uint16_t data_len;
	union {
		uint16_t fee_pkt_type;
		struct fee_pkt_type type;
	};
	uint16_t frame_cntr;	/* increments per-frame, see Draft A.14 MSSL-IF-109 */
	uint16_t seq_cntr;	/* packet seq. in frame transfer, see Draft A.14 MSSL-IF-110 */
} __attribute__((packed));


/* @see MSSL-SMILE-SXI-IRD-0001 Draft A.14, req. MSSL-IF-102 */
#define FEE_EV_COLS		 5
#define FEE_EV_ROWS		 5
#define FEE_EV_DET_PIXELS	25	/* 5x5 grid around event pixel */
#define FEE_EV_PIXEL_IDX	12	/* the index of the event pixel */
#define FEE_EV_DATA_LEN		((2 +  FEE_EV_DET_PIXELS) *  sizeof(uint16_t))

__extension__
struct fee_event_detection {
	struct fee_data_hdr hdr;

	uint16_t col;
	uint16_t row;

	uint16_t pix[FEE_EV_DET_PIXELS];

} __attribute__((packed));



/**
 * number of pixels of the CCD image sections per side of the CCD
 *
 * the pixel sized of the 6x6 binned (=science mode) images consist of
 * visible pixels plus overscan, as the nominal rows/cols
 * do not divide to full integers in binned mode
 *
 * there is no (formal) mention of 24x24 binning mode in the IRD, the
 * values below are just a guess based on the shape of the CCD and the
 * number of samples in pattern mode re returned by the FEE
 *
 *
 * @see SSL-SMILE-SXI-IRD-0001 Draft A.14:
 *		fig. 6-1, 6-3
 *		MSSL-IF-38
 */

/* image and readout sections */
#define FEE_CCD_IMG_SEC_ROWS		3791
#define FEE_CCD_IMG_SEC_COLS		2255
#define FEE_CCD_RDO_SEC_ROWS		 719
#define FEE_CCD_RDO_SEC_COLS		2255
/* binned frame sizes */
#define FEE_EDU_FRAME_6x6_ROWS		 639
#define FEE_EDU_FRAME_6x6_COLS		 384
#define FEE_EDU_FRAME_24x24_ROWS	 160
#define FEE_EDU_FRAME_24x24_COLS	  99



/**
 * the external SRAM stores for CCD E and F data; after readout with 6x6
 * binning, the data should be in these areas, with CCD2 and CCD4 data
 * interleaved, i.e.
 *
 * [31:16]	[15:0]		(start 0x00801800)
 * CCD4_E data	CCD2_E data
 * (...)
 *				(stop 0x00900BFC)
 *
 * and
 *
 * [31:16]	[15:0]		(start 0x00900C00)
 * CCD4_F data	CCD2_F data
 * (...)
 *				(stop 0x00BFFFC)
 *
 *
 * NOTE: values and layout are from an email by Sampie; it is not known why
 *	 the SRAM sizes are soemwhat odd. They could just represent an
 *	 arbitrary minimum size for storing pixel data along with some
 *	 margin
 */

#define FEE_SRAM_SIDE_E_START		0x00801800
#define FEE_SRAM_SIDE_E_STOP		0x00900BFC
#define FEE_SRAM_SIDE_F_START		0x00900C00
#define FEE_SRAM_SIDE_F_STOP		0x00BFFFFC

#define FEE_SRAM_SIDE_E_SIZE		(FEE_SRAM_SIDE_E_STOP - FEE_SRAM_SIDE_E_START + 1UL)
#define FEE_SRAM_SIDE_F_SIZE		(FEE_SRAM_SIDE_F_STOP - FEE_SRAM_SIDE_F_START + 1UL)


/**
 * the external SRAM stores for correction data
 *
 * row correction values:
 * =====================
 * [31:24]	[23:16]		[15:8]		[7:0]		(start 0x00800000)
 * CCD4_E_ROWC	CCD4_F_ROWC	CCD2_E_ROWC	CCD2_F_ROWC
 * (...)
 *								(stop 0x00800FFC)
 *	(1024 words total)
 *
 *
 * column correction values:
 * =====================
 * [31:24]	[23:16]		[15:8]		[7:0]		(start 0x00801000)
 * CCD4_E_COLC	CCD4_F_COLC	CCD2_E_COLC	CCD2_F_COLC
 * (...)
 *								(stop 0x008017FC)
 *	(512 words total)
 *
 */

#define FEE_SRAM_ROW_CORR_START		0x00800000
#define FEE_SRAM_ROW_CORR_STOP		0x00800FFC
#define FEE_SRAM_COL_CORR_START		0x00801000
#define FEE_SRAM_COL_CORR_STOP		0x008017FC


/**
 * the SRAM start and end addresses appears to coincide with the address of
 * FEE_SRAM_ROW_CORR_START and FEE_SRAM_SIDE_F_STOP
 */
#define FEE_SRAM_START			FEE_SRAM_ROW_CORR_START
#define FEE_SRAM_END			(FEE_SRAM_SIDE_F_STOP + 1UL)
#define FEE_SRAM_SIZE			(FEE_SRAM_END - FEE_SRAM_START)


__extension__
struct fee_data_pkt {
	struct fee_data_hdr hdr;
	uint8_t data[];
} __attribute__((packed));

__extension__
struct fee_pattern {
	union {
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
		struct {
			uint16_t time_code:3;
			uint16_t ccd:1;
			uint16_t side:2;
			uint16_t row:5;
			uint16_t col:5;
		};
#elif (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
		struct {
			uint16_t col:5;
			uint16_t row:5;
			uint16_t side:2;
			uint16_t ccd:1;
			uint16_t time_code:3;
		};
#endif
		uint16_t field;
	};

} __attribute__((packed));


/**
 * The HK packet is just a copy of the FEE HK registers. It uses the same
 * header as a data packet and contains 144 payload bytes as of Feb. 23. 2022,
 * but should be 152 bytes as per the register map v0.22.
 */

#define FEE_HK_PACKET_DATA_LEN	152

__extension__
struct fee_hk_data_payload {
	uint8_t hk[FEE_HK_PACKET_DATA_LEN];
} __attribute__((packed));




/**
 * The FT mode frame container structure
 */

struct fee_ft_data {

	struct fee_hk_data_payload hk;

	uint16_t *E2;		/* the readout node buffers, if a buffer is */
	uint16_t *F2;		/* unused, the pointer is NULL */
	uint16_t *E4;
	uint16_t *F4;

	uint16_t *data;

	size_t rows;	/* the rows and columns of each readout node buffer */
	size_t cols;
	size_t bins;	/* the CCD binning mode */

	size_t n_elem;	/* rows * cols */

	size_t n_E2;	/* the number of elements stored in each buffer */
	size_t n_F2;
	size_t n_E4;
	size_t n_F4;

	uint16_t readout;
};


/**
 * The FF mode frame container structure
 */

struct fee_ff_data {

	struct fee_hk_data_payload hk;
	uint16_t ccd_id;

	uint16_t *data;
	size_t n_elem;	/* allocated number of elements (i.e. bufsize) */

	size_t n;	/* actual number of elements */
};





void fee_pkt_hdr_to_cpu(struct fee_data_pkt *pkt);

int fee_pkt_is_event(struct fee_data_pkt *pkt);
int fee_pkt_is_wandering_mask(struct fee_data_pkt *pkt);
void fee_pkt_show_event(struct fee_data_pkt *pkt);
void fee_pkt_show_wandering_mask(struct fee_data_pkt *pkt);
void fee_pkt_event_to_cpu(struct fee_data_pkt *pkt);
void fee_pkt_wandering_mask_to_cpu(struct fee_data_pkt *pkt);

int fee_event_is_xray(struct fee_data_pkt *pkt,
		      uint16_t centre_th, uint32_t sum_th, uint16_t ring_th);
int fee_event_pixel_is_bad(struct fee_data_pkt *pkt);

void fee_ft_aggregator_destroy(struct fee_ft_data *ft);
struct fee_ft_data *fee_ft_aggregator_create(void);
int fee_ft_aggregate(struct fee_ft_data *ft, struct fee_data_pkt *pkt);


void fee_ff_aggregator_destroy(struct fee_ff_data *ff);
struct fee_ff_data *fee_ff_aggregator_create(void);
int fee_ff_aggregate(struct fee_ff_data *ff, struct fee_data_pkt *pkt);



#endif /* SMILE_FEE_H */
