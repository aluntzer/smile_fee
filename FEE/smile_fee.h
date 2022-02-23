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
 */


#define FEE_MODE_ID_ON		0x0	/* the thing is switched on */
#define FEE_MODE_ID_FTP		0x1	/* frame transfer pattern */
#define FEE_MODE_ID_STBY	0x2	/* stand-by-mode */
#define FEE_MODE_ID_FT		0x3	/* frame transfer */
#define FEE_MODE_ID_FF		0x4	/* full frame */
#define FEE_CMD__ID_IMM_ON	0x8	/* immediate on-mode, this is a command, not a mode */
#define FEE_MODE_ID_PTP1	0xB	/* parallel trap pump mode 1 */
#define FEE_MODE_ID_PTP2	0xC	/* parallel trap pump mode 2 */
#define FEE_MODE_ID_STP1	0xD	/* serial trap pump mode 1 */
#define FEE_MODE_ID_STP2	0xE	/* serial trap pump mode 2 */

#define FEE_MODE2_NOBIN		0x1	/* no binning mode */
#define FEE_MODE2_BIN6		0x2	/* 6x6 binning mode */
#define FEE_MODE2_BIN24		0x3	/* 24x4 binning mode */

/* these identifiy the bits in the readout node selection register */
#define FEE_READOUT_NODE_E2	0b0010
#define FEE_READOUT_NODE_F2	0b0001
#define FEE_READOUT_NODE_E4	0b1000
#define FEE_READOUT_NODE_F4	0b0100


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

__extension__
struct fee_event_dection {
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

#define FEE_CCD_IMG_SEC_ROWS		3791
#define FEE_CCD_IMG_SEC_COLS		2255
#define FEE_EDU_FRAME_6x6_ROWS		639
#define FEE_EDU_FRAME_6x6_COLS		384
#define FEE_EDU_FRAME_24x24_ROWS	160
#define FEE_EDU_FRAME_24x24_COLS	 99



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
 * The HK packet structure is completely undocumented. All I currently
 * know is that it uses the same header as a data packet and contains
 * 144 payload bytes as of Feb. 23. 2022
 */

#define FEE_HK_PACKET_DATA_LEN	144

__extension__
struct fee_hk_data_payload {
	uint8_t hk[144];
} __attribute__((packed));



#endif /* SMILE_FEE_H */
