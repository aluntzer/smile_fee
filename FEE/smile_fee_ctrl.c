/**
 * @file   smile_fee_ctrl.c
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   2018
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
 * @brief RMAP SMILE FEE control library
 * @see SMILE-MSSL-PL-Register_map_v0.10_Draft
 *
 * USAGE:
 *
 *	Access to the local mirror is provided by _get or _set calls, so
 *	to configure a register in the SMILE_FEE, one would call the sequence:
 *
 *	set_register_xyz(someargument);
 *	sync_register_xyz_to_fee();
 *
 *	while(smile_fee_sync_status())
 *		wait_busy_or_do_something_else_your_choice();
 *
 *	[sync_comple]
 *
 *	and to read a register:
 *
 *	sync_register_uvw_to_dpu()
 *
 *	while(smile_fee_sync_status())
 *		wait_busy_or_do_something_else_your_choice();
 *
 *	[sync_comple]
 *
 *	value = get_register_uvw();
 *
 *
 * WARNING: This has not been thoroughly tested and is in need of inspection
 *	    with regards to the specification, in order to locate any
 *	    register transcription errors or typos.
 *	    Note that the FEE register layout may have been changed during the
 *	    development process. Make sure to inspect the latest register
 *	    map for changes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rmap.h>
#include <smile_fee_cmd.h>
#include <smile_fee_ctrl.h>
#include <smile_fee_rmap.h>


static struct smile_fee_mirror *smile_fee;



/**
 * @brief get the start of vertical row shared with charge injection
 */

uint16_t smile_fee_get_vstart(void)
{
	return (uint16_t) (smile_fee->cfg_reg_0 & 0xFFFFUL);
}


/**
 * @brief set the start of vertical row shared with charge injection
 */

void smile_fee_set_vstart(uint16_t vstart)
{
	smile_fee->cfg_reg_0 &= ~0xFFFFUL;
	smile_fee->cfg_reg_0 |=  0xFFFFUL & ((uint32_t) vstart);
}

/**
 * @brief get the end of vertical row with charge injection
 */

uint16_t smile_fee_get_vend(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_0 >> 16) & 0xFFFFUL);
}


/**
 * @brief set the end of vertical row with charge injection
 */

void smile_fee_set_vend(uint16_t vend)
{
	smile_fee->cfg_reg_0 &= ~(0xFFFFUL << 16);
	smile_fee->cfg_reg_0 |=  (0xFFFFUL & ((uint32_t) vend)) << 16;
}


/**
 * @brief get the charge injection width
 */

uint16_t smile_fee_get_charge_injection_width(void)
{
	return (uint16_t) (smile_fee->cfg_reg_1 & 0xFFFFUL);
}


/**
 * @brief set the charge injection width
 */

void smile_fee_set_charge_injection_width(uint16_t width)
{
	smile_fee->cfg_reg_1 &= ~0xFFFFUL;
	smile_fee->cfg_reg_1 |=  0xFFFFUL & ((uint32_t) width);
}


/**
 * @brief get the charge injection gap
 */

uint16_t smile_fee_get_charge_injection_gap(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_1 >> 16) & 0xFFFFUL);
}


/**
 * @brief set the charge injection gap
 */

void smile_fee_set_charge_injection_gap(uint16_t gap)
{
	smile_fee->cfg_reg_1 &= ~(0xFFFFUL << 16);
	smile_fee->cfg_reg_1 |=  (0xFFFFUL & ((uint32_t) gap)) << 16;
}

/**
 * @brief get the duration of a parallel overlap period (TOI)
 */

uint16_t smile_fee_get_parallel_toi_period(void)
{
	return (uint16_t) (smile_fee->cfg_reg_2 & 0xFFFUL);
}


/**
 * @brief set the duration of a parallel overlap period (TOI)
 */

void smile_fee_set_parallel_toi_period(uint16_t period)
{
	smile_fee->cfg_reg_2 &= ~0xFFFUL;
	smile_fee->cfg_reg_2 |=  0xFFFUL & ((uint32_t) period);
}


/**
 * @brief get the extra parallel clock overlap
 */

uint16_t smile_fee_get_parallel_clk_overlap(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_2 >> 12) & 0xFFFUL);
}


/**
 * @brief set the extra parallel clock overlap
 */

void smile_fee_set_parallel_clk_overlap(uint16_t overlap)
{
	smile_fee->cfg_reg_2 &= ~(0xFFFUL << 12);
	smile_fee->cfg_reg_2 |=  (0xFFFUL & ((uint32_t) overlap)) << 12;
}


/**
 * @brief get CCD read-out
 *
 * @param ccd_id the id of the CCD (1 == CCD1 , 2 == CCD2, all others reserved)
 *
 * @note 1 indicates that CCD is read-out
 *
 * @returns 1 if ccd is read-out, 0 otherwise; always returns 0 if reserved
 *	    CCD ID is issued
 */

uint32_t smile_fee_get_ccd_readout(uint32_t ccd_id)
{
	if (!ccd_id)
		return 0;

	if (ccd_id > 2)
		return 0;

	/* note: bit index starts at 0, but ccd id starts at 1, hence the
	 * subtraction
	 */
	return (smile_fee->cfg_reg_2 >> 24) & (1 << (ccd_id - 1));
}


/**
 * @brief set CCD read-out
 *
 * @param ccd_id the id of the CCD (1 == CCD1 , 2 == CCD2, all others reserved)
 * @param status the read-out status to set, either 0 or any bit set
 *
 * @note 1 indicates that CCD is read-out
 *
 */

void smile_fee_set_ccd_readout(uint32_t ccd_id, uint32_t status)
{
	if (!ccd_id)
		return;

	if (ccd_id > 2)
		return;

	if (status)
		status = 1;

	/* note: bit index starts at 0, but ccd id starts at 1, hence the
	 * subtraction
	 */

	smile_fee->cfg_reg_2 &= ~((status << (ccd_id - 1)) << 24);
	smile_fee->cfg_reg_2 |=  ((status << (ccd_id - 1)) << 24);
}


/**
 * @brief get the amount of lines to be dumped after readout
 */

uint16_t smile_fee_get_n_final_dump(void)
{
	return (uint16_t) (smile_fee->cfg_reg_3 & 0xFFFFUL);
}


/**
 * @brief set the amount of lines to be dumped after readout
 */

void smile_fee_set_n_final_dump(uint16_t lines)
{
	smile_fee->cfg_reg_3 &= ~0xFFFFUL;
	smile_fee->cfg_reg_3 |=  0xFFFFUL & ((uint32_t) lines);
}


/**
 * @brief get the amount of serial register transfers
 */

uint16_t smile_fee_get_h_end(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_3 >> 16) & 0xFFFUL);
}


/**
 * @brief set the amount of serial register transfers
 */

void smile_fee_set_h_end(uint16_t transfers)
{
	smile_fee->cfg_reg_2 &= ~(0xFFFUL << 16);
	smile_fee->cfg_reg_2 |=  (0xFFFUL & ((uint32_t) transfers)) << 16;
}


/**
 * @brief get charge injection mode
 *
 * @returns 0 if disabled, bits set otherwise
 */

uint32_t smile_fee_get_charge_injection(void)
{
	return (smile_fee->cfg_reg_3 >> 28) & 0x1UL;
}


/**
 * @brief set the charge injection mode
 *
 * @param mode enable/disable injection; either 0 to disable or any bit set to
 *	       enable
 */

void smile_fee_set_charge_injection(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_3 &= ~(0x1UL << 28);
	smile_fee->cfg_reg_3 |=  (mode << 28);
}


/**
 * @brief get parallel clock generation
 *
 * @returns 1 if bi-levels parallel clocks are generated, 0 if tri-level
 *	    parallel clocks are generated
 */

uint32_t smile_fee_get_tri_level_clk(void)
{
	return (smile_fee->cfg_reg_3 >> 29) & 0x1UL;
}


/**
 * @brief set parallel clock generation
 *
 * @param mode set 0 for tri-level parallel clock generation,
 *	       any bit set for bi-level parallel clock generation
 */

void smile_fee_set_tri_level_clk(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_3 &= ~(0x1UL << 29);
	smile_fee->cfg_reg_3 |=  (mode  << 29);
}


/**
 * @brief get image clock direction
 *
 * @returns 1 if reverse parallel clocks are generated, 0 if normal forward
 *	    parallel clocks are generated
 */

uint32_t smile_fee_get_img_clk_dir(void)
{
	return (smile_fee->cfg_reg_3 >> 30) & 0x1UL;
}


/**
 * @brief set image clock direction

 *
 * @param mode set 0 for normal forward parallel clock generation
 *	       any bit set for reverse parallel clock generation
 */

void smile_fee_set_img_clk_dir(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_3 &= ~(0x1UL << 30);
	smile_fee->cfg_reg_3 |=  (mode  << 30);
}


/**
 * @brief get serial clock direction
 *
 * @returns 1 if reverse serial clocks are generated, 0 if normal forward
 *	    serial clocks are generated
 */

uint32_t smile_fee_get_clk_dir(void)
{
	return (smile_fee->cfg_reg_3 >> 31) & 0x1UL;
}


/**
 * @brief set serial clock direction
 *
 * @param mode set 0 for normal forward serial clock generation
 *	       any bit set for reverse serial clock generation
 */

void smile_fee_set_clk_dir(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_3 &= ~(0x1UL << 31);
	smile_fee->cfg_reg_3 |=  (mode  << 31);
}


/**
 * @brief get packet size
 *
 * @returns 10 byte packed header + number of load bytes; number of load bytes
 *	    are multiple of 4.
 */

uint16_t smile_fee_get_packet_size(void)
{
	return (uint16_t) (smile_fee->cfg_reg_4 & 0xFFFFUL);
}

/**
 * @brief set packet size
 *
 * @param pkt_size 10 byte packed header + number of load bytes; number of load
 *		   bytes must be a multiple of 4
 */

void smile_fee_set_packet_size(uint16_t pkt_size)
{
	smile_fee->cfg_reg_4 &= ~0xFFFFUL;
	smile_fee->cfg_reg_4 |=  0xFFFFUL & ((uint32_t) pkt_size);
}


/**
 * @brief get the integration period
 */

uint16_t smile_fee_get_int_period(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_4 >> 16) & 0xFFFFUL);
}


/**
 * @brief set the integration period
 */

void smile_fee_set_int_period(uint16_t period)
{
	smile_fee->cfg_reg_4 &= ~(0xFFFFUL << 16);
	smile_fee->cfg_reg_4 |=  (0xFFFFUL & ((uint32_t) period)) << 16;
}


/**
 * @brief get internal mode sync
 *
 * @returns 1 if internal mode sync is enabvle, 0 if disabled
 */

uint32_t smile_fee_get_sync_sel(void)
{
	return (smile_fee->cfg_reg_5 >> 20) & 0x1UL;
}


/**
 * @brief set internal mode sync
 *
 * @param mode set 0 to disable internal mode sync
 *	       any bit set to enable internal mode sync
 */

void smile_fee_set_sync_sel(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_5 &= ~(0x1UL << 20);
	smile_fee->cfg_reg_5 |=  (mode  << 20);
}


/**
 * @brief get the readout node(s)
 *
 * @returns 0x1 if read-out via F-side
 *          0x2 if read-out via E-side
 *          0x3 if read-out via both F- and E-side
 */

uint16_t smile_fee_get_readout_nodes(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_5 >> 21) & 0x3UL);
}


/**
 * @brief set the readout node(s)
 *
 * @param nodes set 0x1 for read-out via F-side,
 *		    0x2 if read-out via E-side,
 *		    0x3 if read-out via both F- and E-side
 */

void smile_fee_set_readout_nodes(uint32_t nodes)
{
	if (!nodes)
		return;

	if (nodes > 3)
		return;

	smile_fee->cfg_reg_5 &= ~(0x3UL << 21);
	smile_fee->cfg_reg_5 |=  (0x3UL & nodes) << 21;
}


/**
 * @brief get digitise mode
 *
 * @returns 1 if digitise data is transferred to DPU during an image mode,
 *	      else image mode is executed but not pixel data is transferred
 */

uint32_t smile_fee_get_digitise(void)
{
	return (smile_fee->cfg_reg_5 >> 23) & 0x1UL;
}


/**
 * @brief set digitise mode
 *
 * @param mode set 0 to disable pixel data transfer to DPU
 *	       any bit set to enable pixel data transfer
 */

void smile_fee_set_digitise(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_5 &= ~(0x1UL << 23);
	smile_fee->cfg_reg_5 |=  (mode  << 23);
}


/**
 * @brief get correction bypass
 *
 * @returns 1 if no correction is applied,
 *	    else background correction is applied to the read-out pixel data
 */

uint32_t smile_fee_get_correction_bypass(void)
{
	return (smile_fee->cfg_reg_5 >> 24) & 0x1UL;
}


/**
 * @brief set correction bypass
 *
 * @param mode set 0 to enable pixel data background correction
 *	       any bit set to disable background correction
 */

void smile_fee_set_correction_bypass(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_5 &= ~(0x1UL << 24);
	smile_fee->cfg_reg_5 |=  (mode  << 24);
}


/**
 * @brief get DG en
 *
 * @returns 0 if DG operate as per mode
 */

uint32_t smile_fee_get_dg_en(void)
{
	return (smile_fee->cfg_reg_5 >> 25) & 0x1UL;
}


/**
 * @brief set DG en
 *
 * @param mode set 0 to operate DG as per mode
 */

void smile_fee_set_dg_en(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_5 &= ~(0x1UL << 25);
	smile_fee->cfg_reg_5 |=  (mode  << 25);
}


/**
 * @brief get vod_config
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_vod_config(void)
{
	return (uint16_t) (smile_fee->cfg_reg_18 & 0xFFFUL);
}


/**
 * @brief set vod_config
 *
 * @note no description available in register map document
 */

void smile_fee_set_vod_config(uint16_t vod)
{
	smile_fee->cfg_reg_18 &= ~0xFFFUL;
	smile_fee->cfg_reg_18 |=  0xFFFUL & ((uint32_t) vod);
}


/**
 * @brief get ccd1_vrd_config
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_ccd1_vrd_config(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_18 >> 12) & 0xFFFUL);
}

/**
 * @brief set ccd1_vrd_config
 *
 * @note no description available in register map document
 */

void smile_fee_set_ccd1_vrd_config(uint16_t vrd)
{
	smile_fee->cfg_reg_18 &= ~(0xFFFUL << 12);
	smile_fee->cfg_reg_18 |=  (0xFFFUL & ((uint32_t) vrd)) << 12;
}


/**
 * @brief get ccd2_vrd_config
 *
 * @note no description available in register map document
 *
 * @warn this is a case of extreme idiocy from a user's perspective.
 *	 make sure to sync both reg 18 and 19 to DPU before using this function
 */

uint16_t smile_fee_get_ccd2_vrd_config(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_18 >> 24) & 0xFFUL)
			| ((smile_fee->cfg_reg_19 & 0x3UL) << 8);
}

/**
 * @brief set ccd2_vrd_config
 *
 * @note no description available in register map document
 *
 * @warn see smile_fee_get_ccd2_vrd_config; make sure to sync both reg 18 and 19
 *	 to FEE after using this function
 */

void smile_fee_set_ccd2_vrd_config(uint16_t vrd)
{
	smile_fee->cfg_reg_18 &= ~(0xFFUL << 24);
	smile_fee->cfg_reg_18 |=  (0xFFUL & ((uint32_t) vrd)) << 24;

	smile_fee->cfg_reg_19 &= ~0x3UL;
	smile_fee->cfg_reg_19 |=  0x3UL & (((uint32_t) vrd) >> 8);
}


/**
 * @brief get ccd3_vrd_config
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_ccd3_vrd_config(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_19 >> 4) & 0xFFFUL);
}

/**
 * @brief set ccd3_vrd_config
 *
 * @note no description available in register map document
 */

void smile_fee_set_ccd3_vrd_config(uint16_t vrd)
{
	smile_fee->cfg_reg_19 &= ~(0xFFFUL << 4);
	smile_fee->cfg_reg_19 |=  (0xFFFUL & ((uint32_t) vrd)) << 4;
}


/**
 * @brief get ccd4_vrd_config
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_ccd4_vrd_config(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_19 >> 16) & 0xFFFUL);
}


/**
 * @brief set ccd4_vrd_config
 *
 * @note no description available in register map document
 */

void smile_fee_set_ccd4_vrd_config(uint16_t vrd)
{
	smile_fee->cfg_reg_19 &= ~(0xFFFUL << 16);
	smile_fee->cfg_reg_19 |=  (0xFFFUL & ((uint32_t) vrd)) << 16;
}


/**
 * @brief get ccd_vgd_config
 *
 * @note no description available in register map document
 *
 * @warn great, another word boundary overlap...
 *	 make sure to sync both reg 19 and 20 to DPU before using this function
 */

uint16_t smile_fee_get_ccd_vgd_config(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_19 >> 28) & 0x3UL)
			| ((smile_fee->cfg_reg_20 & 0xFFUL) << 3);
}

/**
 * @brief set ccd_vgd_config
 *
 * @note no description available in register map document
 *
 * @warn see smile_fee_get_ccd_vgd_config; make sure to sync both reg 19 and 20
 *	 to FEE after using this function
 */

void smile_fee_set_ccd_vgd_config(uint16_t vgd)
{
	smile_fee->cfg_reg_19 &= ~(0x3UL << 28);
	smile_fee->cfg_reg_19 |=  (0x3UL & ((uint32_t) vgd)) << 28;

	smile_fee->cfg_reg_20 &= ~0xFFUL;
	smile_fee->cfg_reg_20 |=  0xFFUL & (((uint32_t) vgd) >> 3);
}


/**
 * @brief get ccd_vog_config
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_ccd_vog_config(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_20 >> 8) & 0xFFFUL);
}


/**
 * @brief set ccd_vog_config
 *
 * @note no description available in register map document
 */

void smile_fee_set_ccd_vog_config(uint16_t vog)
{
	smile_fee->cfg_reg_20 &= ~(0xFFFUL << 8);
	smile_fee->cfg_reg_20 |=  (0xFFFUL & ((uint32_t) vog)) << 8;
}


/**
 * @brief get ccd_ig_hi_config
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_ccd_ig_hi_config(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_20 >> 20) & 0xFFFUL);
}


/**
 * @brief set ccd_ig_hi_config
 *
 * @note no description available in register map document
 */

void smile_fee_set_ccd_ig_hi_config(uint16_t cfg)
{
	smile_fee->cfg_reg_20 &= ~(0xFFFUL << 20);
	smile_fee->cfg_reg_20 |=  (0xFFFUL & ((uint32_t) cfg)) << 20;
}


/**
 * @brief get ccd_ig_lo_config
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_ccd_ig_lo_config(void)
{
	return (uint16_t) (smile_fee->cfg_reg_21 & 0xFFFUL);
}


/**
 * @brief set ccd_ig_lo_config
 *
 * @note no description available in register map document
 */

void smile_fee_set_ccd_ig_lo_config(uint16_t cfg)
{
	smile_fee->cfg_reg_21 &= ~0xFFFUL;
	smile_fee->cfg_reg_21 |=  0xFFFUL & (uint32_t) cfg;
}


/**
 * @brief get start row number
 */

uint16_t smile_fee_get_h_start(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_21 >> 12) & 0xFFFUL);
}


/**
 * @brief set start row number
 */

void smile_fee_set_h_start(uint16_t row)
{
	smile_fee->cfg_reg_21 &= ~(0xFFFUL << 12);
	smile_fee->cfg_reg_21 |=  (0xFFFUL & ((uint32_t) row)) << 12;
}


/**
 * @brief get next mode of operation
 *
 * @note as per register map document, the following return values
 *	 are currently valid:
 *
 *	 0x0 (On-Mode)
 *	 0x1 (Frame Transfer (FT) Mode Pattern Mode)
 *	 0x2 (Stand-By-Mode)
 *	 0x3 (Frame Transfer Mode(FT))
 *	 0x4 (Full Frame Mode(FF))
 *	 0x5 (Parallel trap pumping mode 1 (FF))
 *	 0x6 (Parallel trap pumping mode 2 (FF))
 *	 0x7 (Serial trap pumping mode 1 (FF))
 *	 0x8 (Serial trap pumping mode 2 (FF))
 *	 0xd (Immediate On-Mode)  (this is a command and not a mode)
 *
 */

uint8_t smile_fee_get_ccd_mode_config(void)
{
	return (uint8_t) ((smile_fee->cfg_reg_21 >> 24) & 0xFUL);
}


/**
 * @brief set next mode of operation
 *
 * @param mode the mode to set
 *
 * @note as per register map document, the following return values
 *	 are currently valid:
 *
 *	 0x0 (On-Mode)
 *	 0x1 (Frame Transfer (FT) Mode Pattern Mode)
 *	 0x2 (Stand-By-Mode)
 *	 0x3 (Frame Transfer Mode(FT))
 *	 0x4 (Full Frame Mode(FF))
 *	 0x5 (Parallel trap pumping mode 1 (FF))
 *	 0x6 (Parallel trap pumping mode 2 (FF))
 *	 0x7 (Serial trap pumping mode 1 (FF))
 *	 0x8 (Serial trap pumping mode 2 (FF))
 *	 0xd (Immediate On-Mode)  (this is a command and not a mode)
 *
 * @warn input parameter is not checked for validity
 */

void smile_fee_set_ccd_mode_config(uint8_t mode)
{
	smile_fee->cfg_reg_21 &= ~(0xFUL << 24);
	smile_fee->cfg_reg_21 |=  (0xFUL & ((uint32_t) mode)) << 24;
}


/**
 * @brief get degree of binning
 *
 *
 * @returns 0x1 if no binning
 *	    0x2 if 6x6 binning
 *	    0x3 if 24x24 binning
 */

uint8_t smile_fee_get_ccd_mode2_config(void)
{
	return (uint8_t) ((smile_fee->cfg_reg_21 >> 28) & 0x3UL);
}


/**
 * @brief set degree of binning
 *
 * @param mode the mode to set:
 *	       0x1 :  no binning
 *             0x2 : 6x6 binning
 *             0x3 : 24x24 binning
 */

void smile_fee_set_ccd_mode2_config(uint8_t mode)
{
	smile_fee->cfg_reg_21 &= ~(0x3UL << 28);
	smile_fee->cfg_reg_21 |=  (0x3UL & ((uint32_t) mode)) << 28;
}


/**
 * @brief get event detection execution
 *
 * @returns 1 if event detection is executed, 0 otherwise
 */

uint32_t smile_fee_get_event_detection(void)
{
	return (smile_fee->cfg_reg_21 >> 30) & 0x1UL;
}


/**
 * @brief set event detection execution
 *
 * @param mode set 0 to disable event detection
 *	       any bit set to enable event detection
 */

void smile_fee_set_event_detection(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_21 &= ~(0x1UL << 30);
	smile_fee->cfg_reg_21 |=  (mode  << 30);
}


/**
 * @brief get error flags clear
 *
 * @returns 1 if flag is set, 0 otherwise
 *
 */

uint32_t smile_fee_get_clear_error_flag(void)
{
	return (smile_fee->cfg_reg_21 >> 31) & 0x1UL;
}


/**
 * @brief set error flags clear
 *
 * @param mode set 0 to disable clear flags
 *	       any bit set to enable clear error flags
 *
 *
 * @warn when set to 1, all error flags generated by the FEE FPGA
 *	 for non-RMAP/SPW related functions are immediately cleared.
 *	 The bit is the reset by the FPGA to re-enable error flags
 *	 BE WARNED: if you set this flag to 1 locally, the FEE will clear all
 *	 flags everytime you sync this register from DPU -> FEE as long
 *	 as the bit is set in the local FEE mirror
 */

void smile_fee_set_clear_error_flag(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_21 &= ~(0x1UL << 31);
	smile_fee->cfg_reg_21 |=  (mode  << 31);
}


/**
 * @brief get ccd2 E single pixel threshold
 *
 * @note this is the threshold above which a pixel may be considered a possible
 *	 soft X-ray event
 */

uint16_t smile_fee_get_ccd2_e_pix_treshold(void)
{
	return (uint16_t) (smile_fee->cfg_reg_22 & 0xFFFFUL);
}


/**
 * @brief set ccd2 E single pixel threshold
 *
 * @param threshold the threshold above which a pixel may be considered
 *		    a possible soft X-ray event
 */

void smile_fee_set_ccd2_e_pix_treshold(uint16_t threshold)
{
	smile_fee->cfg_reg_22 &= ~0xFFFFUL;
	smile_fee->cfg_reg_22 |=  0xFFFFUL & ((uint32_t) threshold);
}


/**
 * @brief get ccd2 F single pixel threshold
 *
 * @note this is the threshold above which a pixel may be considered a possible
 *	 soft X-ray event
 */

uint16_t smile_fee_get_ccd2_f_pix_treshold(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_22 >> 16) & 0xFFFFUL);
}


/**
 * @brief set ccd2 F single pixel threshold
 *
 * @param threshold the threshold above which a pixel may be considered
 *		    a possible soft X-ray event
 */

void smile_fee_set_ccd2_f_pix_treshold(uint16_t threshold)
{
	smile_fee->cfg_reg_22 &= ~(0xFFFFUL << 16);
	smile_fee->cfg_reg_22 |=  (0xFFFFUL & ((uint32_t) threshold)) << 16;
}


/**
 * @brief get ccd4 E single pixel threshold
 *
 * @note this is the threshold above which a pixel may be considered a possible
 *	 soft X-ray event
 */

uint16_t smile_fee_get_ccd4_e_pix_treshold(void)
{
	return (uint16_t) (smile_fee->cfg_reg_23 & 0xFFFFUL);
}


/**
 * @brief set ccd4 E single pixel threshold
 *
 * @param threshold the threshold above which a pixel may be considered
 *		    a possible soft X-ray event
 */

void smile_fee_set_ccd4_e_pix_treshold(uint16_t threshold)
{
	smile_fee->cfg_reg_23 &= ~0xFFFFUL;
	smile_fee->cfg_reg_23 |=  0xFFFFUL & ((uint32_t) threshold);
}


/**
 * @brief get ccd4 F single pixel threshold
 *
 * @note this is the threshold above which a pixel may be considered a possible
 *	 soft X-ray event
 */

uint16_t smile_fee_get_ccd4_f_pix_treshold(void)
{
	return (uint16_t) ((smile_fee->cfg_reg_23 >> 16) & 0xFFFFUL);
}


/**
 * @brief set ccd4 F single pixel threshold
 *
 * @param threshold the threshold above which a pixel may be considered
 *		    a possible soft X-ray event
 */

void smile_fee_set_ccd4_f_pix_treshold(uint16_t threshold)
{
	smile_fee->cfg_reg_23 &= ~(0xFFFFUL << 16);
	smile_fee->cfg_reg_23 |=  (0xFFFFUL & ((uint32_t) threshold)) << 16;
}


/**
 * @brief get pixel offset value
 *
 * @note his offset value is added to the average incoming pixel value
 */

uint8_t smile_fee_get_pix_offset(void)
{
	return (uint8_t) (smile_fee->cfg_reg_24 & 0xFFUL);
}


/**
 * @brief set pixel offset value
 *
 * @offset  the offset value to be added to the average incoming pixel value
 */

void smile_fee_set_pix_offset(uint8_t offset)
{
	smile_fee->cfg_reg_24 &= ~0xFFUL;
	smile_fee->cfg_reg_24 |=  0xFFUL & ((uint32_t) offset);
}


/**
 * @brief get event packet limit
 *
 * @note this is the total number of even packet per CCD that will be transmitted
 */

uint32_t smile_fee_get_event_pkt_limit(void)
{
	return (uint32_t) ((smile_fee->cfg_reg_24 >> 8) & 0xFFFFFUL);
}


/**
 * @brief set event packet limit
 *
 * @param pkt_limit the total number of even packet per CCD that will be transmitted
 */

void smile_fee_set_event_pkt_limit(uint32_t pkt_limit)
{
	smile_fee->cfg_reg_24 &= ~(0xFFFFFFUL << 8);
	smile_fee->cfg_reg_23 |=  (0xFFFFFFUL & ((uint32_t) pkt_limit)) << 8;
}


/**
 * @brief get execute op flag
 *
 * @returns 1 if execute op flag is set, 0 otherwise
 *
 * @note if this flag is set, the set operational parameters are expedited
 * @warn the register map document does not detail any behaviour, i.e. whether
 *	 this flag clears itself or must be cleared by the user
 *
 */

uint32_t smile_fee_get_execute_op(void)
{
	return smile_fee->cfg_reg_25 & 0x1UL;
}


/**
 * @brief set execute op flag
 *
 * @param mode set 0 to disable execute op
 *	       any bit set to enable execute op
 *
 * @note if this flag is set, the set operational parameters are expedited
 * @warn the register map document does not detail any behaviour, i.e. whether
 *	 this flag clears itself or must be cleared by the user
 *	 ASSUME YOU HAVE TO CLEAR THIS EXPLICITLY BEFORE CHANGING PARAMETERS OR
 *	 EXECUTING A DPU->FEE SYNC
 */

void smile_fee_set_execute_op(uint32_t mode)
{
	if (mode)
		mode = 1;


	smile_fee->cfg_reg_25 &= ~0x1UL;
	smile_fee->cfg_reg_25 |= mode;
}


/**
 * @brief get CCD1_TS HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd1_ts(void)
{
	return (uint16_t) (smile_fee->hk_reg_3 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD1_TS HK value
 */

void smile_fee_set_hk_ccd1_ts(uint16_t ccd1_ts)
{
	smile_fee->hk_reg_3 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_3 |= (0xFFFFUL & ((uint32_t) ccd1_ts)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD2_TS HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd2_ts(void)
{
	return (uint16_t) smile_fee->hk_reg_3 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD2_TS HK value
 */

void smile_fee_set_hk_ccd2_ts(uint16_t ccd2_ts)
{
	smile_fee->hk_reg_3 &= ~0xFFFFUL;
	smile_fee->hk_reg_3 |= 0xFFFFUL & (uint32_t) ccd2_ts;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD3_TS HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd3_ts(void)
{
	return (uint16_t) (smile_fee->hk_reg_4 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD3_TS HK value
 */

void smile_fee_set_hk_ccd3_ts(uint16_t ccd3_ts)
{
	smile_fee->hk_reg_4 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_4 |= (0xFFFFUL & ((uint32_t) ccd3_ts)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD4_TS HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd4_ts(void)
{
	return (uint16_t) smile_fee->hk_reg_4 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD4_TS HK value
 */

void smile_fee_set_hk_ccd4_ts(uint16_t ccd4_ts)
{
	smile_fee->hk_reg_4 &= ~0xFFFFUL;
	smile_fee->hk_reg_4 |= 0xFFFFUL & (uint32_t) ccd4_ts;
}
#endif /* FEE_SIM */


/**
 * @brief get PRT1 HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_prt1(void)
{
	return (uint16_t) (smile_fee->hk_reg_5 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set PRT1 HK value
 */

void smile_fee_set_hk_prt1(uint16_t prt1)
{
	smile_fee->hk_reg_5 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_5 |= (0xFFFFUL & ((uint32_t) prt1)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get PRT2 HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_prt2(void)
{
	return (uint16_t) smile_fee->hk_reg_5 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set PRT2 HK value
 */

void smile_fee_set_hk_prt2(uint16_t prt2)
{
	smile_fee->hk_reg_5 &= ~0xFFFFUL;
	smile_fee->hk_reg_5 |= 0xFFFFUL & (uint32_t) prt2;
}
#endif /* FEE_SIM */


/**
 * @brief get PRT3 HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_prt3(void)
{
	return (uint16_t) (smile_fee->hk_reg_6 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set PRT3 HK value
 */

void smile_fee_set_hk_prt3(uint16_t prt3)
{
	smile_fee->hk_reg_6 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_6 |= (0xFFFFUL & ((uint32_t) prt3)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get PRT4 HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_prt4(void)
{
	return (uint16_t) smile_fee->hk_reg_6 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set PRT4 HK value
 */

void smile_fee_set_hk_prt4(uint16_t prt4)
{
	smile_fee->hk_reg_6 &= ~0xFFFFUL;
	smile_fee->hk_reg_6 |= 0xFFFFUL & (uint32_t) prt4;
}
#endif /* FEE_SIM */


/**
 * @brief get PRT5_TS HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_prt5(void)
{
	return (uint16_t) (smile_fee->hk_reg_7 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set PRT5 HK value
 */

void smile_fee_set_hk_prt5(uint16_t prt5)
{
	smile_fee->hk_reg_7 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_7 |= (0xFFFFUL & ((uint32_t) prt5)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get ZERO_DIFF_AMP HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_zero_diff_amp(void)
{
	return (uint16_t) smile_fee->hk_reg_7 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set ZERO_DIFF_AMP HK value
 */

void smile_fee_set_hk_zero_diff_amp(uint16_t zero_diff_amp)
{
	smile_fee->hk_reg_7 &= ~0xFFFFUL;
	smile_fee->hk_reg_7 |= 0xFFFFUL & (uint32_t) zero_diff_amp;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD3_VOD_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd3_vod_mon(void)
{
	return (uint16_t) (smile_fee->hk_reg_11 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD3_VOD_MON HK value
 */

void smile_fee_set_hk_ccd3_vod_mon(uint16_t ccd3_vod_mon)
{
	smile_fee->hk_reg_11 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_11 |= (0xFFFFUL & ((uint32_t) ccd3_vod_mon)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD3_VOG_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd3_vog_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_11 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD3_VOG_MON HK value
 */

void smile_fee_set_hk_ccd3_vog_mon(uint16_t ccd3_vog_mon)
{
	smile_fee->hk_reg_11 &= ~0xFFFFUL;
	smile_fee->hk_reg_11 |= 0xFFFFUL & (uint32_t) ccd3_vog_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD3_VRD_MON_E HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd3_vrd_mon_e(void)
{
	return (uint16_t) (smile_fee->hk_reg_12 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD3_VRD_MON_E HK value
 */

void smile_fee_set_hk_ccd3_vrd_mon_e(uint16_t ccd3_vrd_mon_e)
{
	smile_fee->hk_reg_12 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_12 |= (0xFFFFUL & ((uint32_t) ccd3_vrd_mon_e)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD4_VOD_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd4_vod_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_12 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD4_VOD_MON HK value
 */

void smile_fee_set_hk_ccd4_vod_mon(uint16_t ccd4_vod_mon)
{
	smile_fee->hk_reg_12 &= ~0xFFFFUL;
	smile_fee->hk_reg_12 |= 0xFFFFUL & (uint32_t) ccd4_vod_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD4_VOG_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd4_vog_mon(void)
{
	return (uint16_t) (smile_fee->hk_reg_13 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD4_VOG_MON HK value
 */

void smile_fee_set_hk_ccd4_vog_mon(uint16_t ccd4_vog_mon)
{
	smile_fee->hk_reg_13 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_13 |= (0xFFFFUL & ((uint32_t) ccd4_vog_mon)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD4_VRD_MON_E HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd4_vrd_mon_e(void)
{
	return (uint16_t) smile_fee->hk_reg_13 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD4_VRD_MON_E HK value
 */

void smile_fee_set_hk_ccd4_vrd_mon_e(uint16_t ccd4_vrd_mon_e)
{
	smile_fee->hk_reg_13 &= ~0xFFFFUL;
	smile_fee->hk_reg_13 |= 0xFFFFUL & (uint32_t) ccd4_vrd_mon_e;
}
#endif /* FEE_SIM */


/**
 * @brief get VCCD HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_vccd(void)
{
	return (uint16_t) (smile_fee->hk_reg_14 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VCCD HK value
 */

void smile_fee_set_hk_vccd(uint16_t vccd)
{
	smile_fee->hk_reg_14 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_14 |= (0xFFFFUL & ((uint32_t) vccd)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get VRCLK_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_vrclk_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_14 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VRCLK_MON HK value
 */

void smile_fee_set_hk_vrclk_mon(uint16_t vrclk_mon)
{
	smile_fee->hk_reg_14 &= ~0xFFFFUL;
	smile_fee->hk_reg_14 |= 0xFFFFUL & (uint32_t) vrclk_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get VICLK HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_viclk(void)
{
	return (uint16_t) (smile_fee->hk_reg_15 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VICLK HK value
 */

void smile_fee_set_hk_viclk(uint16_t viclk)
{
	smile_fee->hk_reg_15 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_15 |= (0xFFFFUL & ((uint32_t) viclk)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get VRCLK_LOW HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_vrclk_low(void)
{
	return (uint16_t) smile_fee->hk_reg_15 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VRCLK_LOW HK value
 */

void smile_fee_set_hk_vrclk_low(uint16_t vrclk_low)
{
	smile_fee->hk_reg_15 &= ~0xFFFFUL;
	smile_fee->hk_reg_15 |= 0xFFFFUL & (uint32_t) vrclk_low;
}
#endif /* FEE_SIM */



/**
 * @brief get 5VB_POS_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_5vb_pos_mon(void)
{
	return (uint16_t) (smile_fee->hk_reg_16 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set 5VB_POS_MON HK value
 */

void smile_fee_set_hk_5vb_pos_mon(uint16_t _5vb_pos_mon)
{
	smile_fee->hk_reg_16 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_16 |= (0xFFFFUL & ((uint32_t) _5vb_pos_mon)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get 5VB_NEG_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_5vb_neg_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_16 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set 5VB_NEG_MON HK value
 */

void smile_fee_set_hk_5vb_neg_mon(uint16_t _5vb_neg_mon)
{
	smile_fee->hk_reg_16 &= ~0xFFFFUL;
	smile_fee->hk_reg_16 |= 0xFFFFUL & (uint32_t) _5vb_neg_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get 3V3B_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_3v3b_mon(void)
{
	return (uint16_t) (smile_fee->hk_reg_17 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set 3V3B_MON HK value
 */

void smile_fee_set_hk_3v3b_mon(uint16_t _3v3b_mon)
{
	smile_fee->hk_reg_17 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_17 |= (0xFFFFUL & ((uint32_t) _3v3b_mon)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get 2V5A_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_2v5a_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_17 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set 2V5A_MON HK value
 */

void smile_fee_set_hk_2v5a_mon(uint16_t _2v5a_mon)
{
	smile_fee->hk_reg_17 &= ~0xFFFFUL;
	smile_fee->hk_reg_17 |= 0xFFFFUL & (uint32_t) _2v5a_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get 3V3D_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_3v3d_mon(void)
{
	return (uint16_t) (smile_fee->hk_reg_18 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set 3V3D_MON HK value
 */

void smile_fee_set_hk_3v3d_mon(uint16_t _3v3d_mon)
{
	smile_fee->hk_reg_18 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_18 |= (0xFFFFUL & ((uint32_t) _3v3d_mon)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get 2V5D_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_2v5d_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_18 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set 2V5D_MON HK value
 */

void smile_fee_set_hk_2v5d_mon(uint16_t _2v5d_mon)
{
	smile_fee->hk_reg_18 &= ~0xFFFFUL;
	smile_fee->hk_reg_18 |= 0xFFFFUL & (uint32_t) _2v5d_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get 1V5D_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_1v5d_mon(void)
{
	return (uint16_t) (smile_fee->hk_reg_19 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set 1V5D_MON HK value
 */

void smile_fee_set_hk_1v5d_mon(uint16_t _1v5d_mon)
{
	smile_fee->hk_reg_19 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_19 |= (0xFFFFUL & ((uint32_t) _1v5d_mon)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get 5VREF_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_5vref_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_19 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set 5VREF_MON HK value
 */

void smile_fee_set_hk_5vref_mon(uint16_t _5vref_mon)
{
	smile_fee->hk_reg_19 &= ~0xFFFFUL;
	smile_fee->hk_reg_19 |= 0xFFFFUL & (uint32_t) _5vref_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get VCCD_POS_RAW HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_vccd_pos_raw(void)
{
	return (uint16_t) (smile_fee->hk_reg_20 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VCCD_POS_RAW HK value
 */

void smile_fee_set_hk_vccd_pos_raw(uint16_t vccd_pos_raw)
{
	smile_fee->hk_reg_20 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_20 |= (0xFFFFUL & ((uint32_t) vccd_pos_raw)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get VCLK_POS_RAW HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_vclk_pos_raw(void)
{
	return (uint16_t) smile_fee->hk_reg_20 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VCLK_POS_RAW HK value
 */

void smile_fee_set_hk_vclk_pos_raw(uint16_t vclk_pos_raw)
{
	smile_fee->hk_reg_20 &= ~0xFFFFUL;
	smile_fee->hk_reg_20 |= 0xFFFFUL & (uint32_t) vclk_pos_raw;
}
#endif /* FEE_SIM */


/**
 * @brief get VAN1_POS_RAW HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_van1_pos_raw(void)
{
	return (uint16_t) (smile_fee->hk_reg_21 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VAN1_POS_RAW HK value
 */

void smile_fee_set_hk_van1_pos_raw(uint16_t van1_pos_raw)
{
	smile_fee->hk_reg_21 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_21 |= (0xFFFFUL & ((uint32_t) van1_pos_raw)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get VAN3_NEG_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_van3_neg_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_21 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VAN3_NEG_MON HK value
 */

void smile_fee_set_hk_van3_neg_raw(uint16_t van3_neg_raw)
{
	smile_fee->hk_reg_21 &= ~0xFFFFUL;
	smile_fee->hk_reg_21 |= 0xFFFFUL & (uint32_t) van3_neg_raw;
}
#endif /* FEE_SIM */



/**
 * @brief get VAN2_POS_RAW HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_van2_pos_raw(void)
{
	return (uint16_t) (smile_fee->hk_reg_22 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VAN2_POS_RAW HK value
 */

void smile_fee_set_hk_van2_pos_raw(uint16_t van2_pos_raw)
{
	smile_fee->hk_reg_22 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_22 |= (0xFFFFUL & ((uint32_t) van2_pos_raw)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get VDIG_RAW HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_vdig_raw(void)
{
	return (uint16_t) smile_fee->hk_reg_22 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VDIG_RAW HK value
 */

void smile_fee_set_hk_vdig_raw(uint16_t vdig_raw)
{
	smile_fee->hk_reg_22 &= ~0xFFFFUL;
	smile_fee->hk_reg_22 |= 0xFFFFUL & (uint32_t) vdig_raw;
}
#endif /* FEE_SIM */

/**
 * @brief get VDIG_RAW_2 HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_vdig_raw_2(void)
{
	return (uint16_t) (smile_fee->hk_reg_23 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VAN2_POS_RAW HK value
 */

void smile_fee_set_hk_vdig_raw_2(uint16_t vdig_raw_2)
{
	smile_fee->hk_reg_23 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_23 |= (0xFFFFUL & ((uint32_t) vdig_raw_2)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get VICLK_RAW HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_viclk_low(void)
{
	return (uint16_t) smile_fee->hk_reg_23 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set VDIG_RAW HK value
 */

void smile_fee_set_hk_viclk_low(uint16_t viclk_low)
{
	smile_fee->hk_reg_23 &= ~0xFFFFUL;
	smile_fee->hk_reg_23 |= 0xFFFFUL & (uint32_t) viclk_low;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD3_VRD_MON_F HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd3_vrd_mon_f(void)
{
	return (uint16_t) (smile_fee->hk_reg_27 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD3_VRD_MON_F HK value
 */

void smile_fee_set_hk_ccd3_vrd_mon_f(uint16_t ccd3_vrd_mon_f)
{
	smile_fee->hk_reg_27 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_27 |= (0xFFFFUL & ((uint32_t) ccd3_vrd_mon_f)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD3_VDD_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd3_vdd_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_27 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD3_VDD_MON HK value
 */

void smile_fee_set_hk_ccd3_vdd_mon(uint16_t ccd3_vdd_mon)
{
	smile_fee->hk_reg_27 &= ~0xFFFFUL;
	smile_fee->hk_reg_27 |= 0xFFFFUL & (uint32_t) ccd3_vdd_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD3_VGD_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd3_vgd_mon(void)
{
	return (uint16_t) (smile_fee->hk_reg_28 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD3_VGD_MON HK value
 */

void smile_fee_set_hk_ccd3_vgd_mon(uint16_t ccd3_vgd_mon)
{
	smile_fee->hk_reg_28 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_28 |= (0xFFFFUL & ((uint32_t) ccd3_vgd_mon)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD4_VRD_MON_F HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd4_vrd_mon_f(void)
{
	return (uint16_t) smile_fee->hk_reg_28 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD4_VRD_MON HK value
 */

void smile_fee_set_hk_ccd4_vrd_mon(uint16_t ccd4_vrd_mon)
{
	smile_fee->hk_reg_28 &= ~0xFFFFUL;
	smile_fee->hk_reg_28 |= 0xFFFFUL & (uint32_t) ccd4_vrd_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD4_VDD_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd4_vdd_mon(void)
{
	return (uint16_t) (smile_fee->hk_reg_29 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD4_VDD_MON HK value
 */

void smile_fee_set_hk_ccd4_vdd_mon(uint16_t ccd4_vdd_mon)
{
	smile_fee->hk_reg_29 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_29 |= (0xFFFFUL & ((uint32_t) ccd4_vdd_mon)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get CCD4_VGD_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ccd4_vgd_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_29 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set CCD4_VGD_MON HK value
 */

void smile_fee_set_hk_ccd4_vgd_mon(uint16_t ccd4_vgd_mon)
{
	smile_fee->hk_reg_29 &= ~0xFFFFUL;
	smile_fee->hk_reg_29 |= 0xFFFFUL & (uint32_t) ccd4_vgd_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get IG_HI_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ig_hi_mon(void)
{
	return (uint16_t) (smile_fee->hk_reg_30 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set IG_HI_MON HK value
 */

void smile_fee_set_hk_ig_hi_mon(uint16_t ig_hi_mon)
{
	smile_fee->hk_reg_30 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_30 |= (0xFFFFUL & ((uint32_t) ig_hi_mon)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get IG_LO_MON HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_ig_lo_mon(void)
{
	return (uint16_t) smile_fee->hk_reg_30 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set IG_LO_MON HK value
 */

void smile_fee_set_hk_ig_lo_mon(uint16_t ig_lo_mon)
{
	smile_fee->hk_reg_30 &= ~0xFFFFUL;
	smile_fee->hk_reg_30 |= 0xFFFFUL & (uint32_t) ig_lo_mon;
}
#endif /* FEE_SIM */


/**
 * @brief get TENSE_A HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_tense_a(void)
{
	return (uint16_t) (smile_fee->hk_reg_31 >> 16) & 0xFFFFUL;

}


#ifdef FEE_SIM
/**
 * @brief set TENSE_A HK value
 */

void smile_fee_set_hk_tense_a(uint16_t tense_a)
{
	smile_fee->hk_reg_31 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_31 |= (0xFFFFUL & ((uint32_t) tense_a)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get TENSE_B HK value
 *
 * @note no description available in register map document
 */

uint16_t smile_fee_get_hk_tense_b(void)
{
	return (uint16_t) smile_fee->hk_reg_31 & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set TENSE_B HK value
 */

void smile_fee_set_hk_tense_b(uint16_t tense_b)
{
	smile_fee->hk_reg_31 &= ~0xFFFFUL;
	smile_fee->hk_reg_31 |= 0xFFFFUL & (uint32_t) tense_b;
}
#endif /* FEE_SIM */


/**
 * @brief get SpW Time Code HK value
 */

uint8_t smile_fee_get_hk_timecode_from_spw(void)
{
	return (uint8_t) (smile_fee->hk_reg_32 >> 16) & 0xFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set SpW Time Code HK value
 */

void smile_fee_set_hk_timecode_from_spw(uint8_t timecode_from_spw)
{
	smile_fee->hk_reg_32 &= ~(0xFFUL << 16);
	smile_fee->hk_reg_32 |= (0xFFUL & ((uint32_t) timecode_from_spw)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get RMAP target status HK value
 */

uint8_t smile_fee_get_hk_rmap_target_status(void)
{
	return (uint8_t) (smile_fee->hk_reg_32 >> 8) & 0xFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set RMAP target status HK value
 */

void smile_fee_set_hk_rmap_target_status(uint8_t rmap_target_status)
{
	smile_fee->hk_reg_32 &= ~(0xFFUL << 8);
	smile_fee->hk_reg_32 |= (0xFFUL & ((uint32_t) rmap_target_status)) << 8;
}
#endif /* FEE_SIM */


/**
 * @brief get RMAP target indicate HK value
 *
 * @returns 1 if set, 0 otherwise
 */

uint32_t smile_fee_get_hk_rmap_target_indicate(void)
{
	return (smile_fee->hk_reg_32 >> 5) & 0x1UL;
}


#ifdef FEE_SIM
/**
 * @brief set RMAP target indicate HK value
 *
 * @param rmap_target_indicate either 0 or any bit set (== 1)
 */

void smile_fee_set_hk_rmap_target_indicate(uint32_t rmap_target_indicate)
{
	if (rmap_target_indicate)
		rmap_target_indicate = 1;

	smile_fee->hk_reg_32 &= ~(0x1UL << 5);
	smile_fee->hk_reg_32 |= rmap_target_indicate << 5;
}
#endif /* FEE_SIM */


/**
 * @brief get SpW link escape error HK value
 *
 * @returns 1 if set, 0 otherwise
 */

uint32_t smile_fee_get_hk_spw_link_escape_error(void)
{
	return (smile_fee->hk_reg_32 >> 4) & 0x1UL;
}


#ifdef FEE_SIM
/**
 * @brief set SpW link escape error HK value
 *
 * @param spw_link_escape_error either 0 or any bit set (== 1)
 */

void smile_fee_set_hk_spw_link_escape_error(uint32_t spw_link_escape_error)
{
	if (spw_link_escape_error)
		spw_link_escape_error = 1;

	smile_fee->hk_reg_32 &= ~(0x1UL << 4);
	smile_fee->hk_reg_32 |= spw_link_escape_error << 4;
}
#endif /* FEE_SIM */


/**
 * @brief get SpW link credit error HK value
 *
 * @returns 1 if set, 0 otherwise
 */

uint32_t smile_fee_get_hk_spw_link_credit_error(void)
{
	return (smile_fee->hk_reg_32 >> 3) & 0x1UL;
}


#ifdef FEE_SIM
/**
 * @brief set SpW link credit error HK value
 *
 * @param spw_link_credit_error either 0 or any bit set (== 1)
 */

void smile_fee_set_hk_spw_link_credit_error(uint32_t spw_link_credit_error)
{
	if (spw_link_credit_error)
		spw_link_credit_error = 1;

	smile_fee->hk_reg_32 &= ~(0x1UL << 3);
	smile_fee->hk_reg_32 |= spw_link_credit_error << 3;
}
#endif /* FEE_SIM */


/**
 * @brief get SpW link parity error HK value
 *
 * @returns 1 if set, 0 otherwise
 */

uint32_t smile_fee_get_hk_spw_link_parity_error(void)
{
	return (smile_fee->hk_reg_32 >> 2) & 0x1UL;
}


#ifdef FEE_SIM
/**
 * @brief set SpW link parity error HK value
 *
 * @param spw_link_parity_error either 0 or any bit set (== 1)
 */

void smile_fee_set_hk_spw_link_parity_error(uint32_t spw_link_parity_error)
{
	if (spw_link_parity_error)
		spw_link_parity_error = 1;

	smile_fee->hk_reg_32 &= ~(0x1UL << 2);
	smile_fee->hk_reg_32 |= spw_link_parity_error << 2;
}
#endif /* FEE_SIM */


/**
 * @brief get SpW link disconnect HK value
 *
 * @returns 1 if set, 0 otherwise
 */

uint32_t smile_fee_get_hk_spw_link_disconnect(void)
{
	return (smile_fee->hk_reg_32 >> 1) & 0x1UL;
}


#ifdef FEE_SIM
/**
 * @brief set SpW link disconnect error HK value
 *
 * @param spw_link_disconnect_error either 0 or any bit set (== 1)
 */

void smile_fee_set_hk_spw_link_disconnect_error(uint32_t spw_link_disconnect_error)
{
	if (spw_link_disconnect_error)
		spw_link_disconnect_error = 1;

	smile_fee->hk_reg_32 &= ~(0x1UL << 1);
	smile_fee->hk_reg_32 |= spw_link_disconnect_error << 1;
}
#endif /* FEE_SIM */


/**
 * @brief get SpW link running HK value
 *
 * @returns 1 if set, 0 otherwise
 */

uint32_t smile_fee_get_hk_spw_link_running(void)
{
	return smile_fee->hk_reg_32 & 0x1UL;
}


#ifdef FEE_SIM
/**
 * @brief set SpW link running error HK value
 *
 * @param spw_link_running_error either 0 or any bit set (== 1)
 */

void smile_fee_set_hk_spw_link_running_error(uint32_t spw_link_running_error)
{
	if (spw_link_running_error)
		spw_link_running_error = 1;

	smile_fee->hk_reg_32 &= ~0x1UL;
	smile_fee->hk_reg_32 |= spw_link_running_error;
}
#endif /* FEE_SIM */


/**
 * @brief get frame counter HK value
 *
 * @note incrementing after each sync25 (description from register map document)
 */

uint32_t smile_fee_get_hk_frame_counter(void)
{
	return (smile_fee->hk_reg_33 >> 16) & 0xFFFFUL;
}


#ifdef FEE_SIM
/**
 * @brief set frame counter HK value
 */

void smile_fee_set_hk_frame_counter(uint16_t frame_counter)
{
	smile_fee->hk_reg_33 &= ~(0xFFFFUL << 16);
	smile_fee->hk_reg_33 |= (0xFFFFUL & ((uint32_t) frame_counter)) << 16;
}
#endif /* FEE_SIM */


/**
 * @brief get FPGA op mode HK value
 */

uint8_t smile_fee_get_hk_fpga_op_mode(void)
{
	return (uint8_t) smile_fee->hk_reg_33 & 0x7FUL;
}


#ifdef FEE_SIM
/**
 * @brief set FPGA op mode HK value
 */

void smile_fee_set_hk_fpga_op_mode(uint8_t fpga_op_mode)
{
	smile_fee->hk_reg_33 &= ~0x7FUL;
	smile_fee->hk_reg_33 |= 0x7FUL & (uint32_t) fpga_op_mode;
}
#endif /* FEE_SIM */


/**
 * @brief get error flag SpW link escape error HK value
 *
 * @returns 1 if set, 0 otherwise
 */

uint32_t smile_fee_get_hk_error_flag_spw_link_escape_error(void)
{
	return smile_fee->hk_reg_34 & 0x1UL;
}


#ifdef FEE_SIM
/**
 * @brief set error flag SpW link escape error HK value
 *
 * @param error_flag_spw_link_escape_error either 0 or any bit set (== 1)
 */

void smile_fee_set_hk_error_flag_spw_link_escape_error(uint32_t error_flag_spw_link_escape_error)
{
	if (error_flag_spw_link_escape_error)
		error_flag_spw_link_escape_error = 1;

	smile_fee->hk_reg_34 &= ~0x1UL;
	smile_fee->hk_reg_34 |= error_flag_spw_link_escape_error;
}
#endif /* FEE_SIM */


/**
 * @brief get error flag SpW link credit error HK value
 *
 * @returns 1 if set, 0 otherwise
 */

uint32_t smile_fee_get_hk_error_flag_spw_link_credit_error(void)
{
	return (smile_fee->hk_reg_34 >> 1) & 0x1UL;
}


#ifdef FEE_SIM
/**
 * @brief set error flag SpW link credit error HK value
 *
 * @param error_flag_spw_link_credit_error either 0 or any bit set (== 1)
 */

void smile_fee_set_hk_error_flag_spw_link_credit_error(uint32_t error_flag_spw_link_credit_error)
{
	if (error_flag_spw_link_credit_error)
		error_flag_spw_link_credit_error = 1;

	smile_fee->hk_reg_34 &= ~(0x1UL << 1);
	smile_fee->hk_reg_34 |= error_flag_spw_link_credit_error << 1;
}
#endif /* FEE_SIM */


/**
 * @brief get error flag SpW link parity error HK value
 *
 * @returns 1 if set, 0 otherwise
 */

uint32_t smile_fee_get_hk_error_flag_spw_link_parity_error(void)
{
	return (smile_fee->hk_reg_34 >> 2) & 0x1UL;
}


#ifdef FEE_SIM
/**
 * @brief set error flag SpW link credit error HK value
 *
 * @param error_flag_spw_link_parity_error either 0 or any bit set (== 1)
 */

void smile_fee_set_hk_error_flag_spw_link_parity_error(uint32_t error_flag_spw_link_parity_error)
{
	if (error_flag_spw_link_parity_error)
		error_flag_spw_link_parity_error = 1;

	smile_fee->hk_reg_34 &= ~(0x1UL << 2);
	smile_fee->hk_reg_34 |= error_flag_spw_link_parity_error << 2;
}
#endif /* FEE_SIM */


/**
 * @brief get FPGA minor version HK value
 *
 * @returns the FPGA minor version
 */

uint8_t smile_fee_get_hk_fpga_minor_version(void)
{
	return (uint8_t) (smile_fee->hk_reg_35 & 0xFFUL);
}


#ifdef FEE_SIM

/**
 * @brief set FPGA minor version HK value
 *
 * @param minor the FPGA minor version
 */

void smile_fee_set_hk_fpga_minor_version(uint8_t minor)
{
	smile_fee->hk_reg_35 &= ~0xFFUL;
	smile_fee->hk_reg_35 |=  0xFFUL & ((uint32_t) minor);
}
#endif /* FEE_SIM */


/**
 * @brief get FPGA major version HK value
 *
 * @returns the FPGA major version
 */

uint8_t smile_fee_get_hk_fpga_major_version(void)
{
	return (uint8_t) ((smile_fee->hk_reg_35 >> 8) & 0xFUL);
}


#ifdef FEE_SIM

/**
 * @brief set FPGA major version HK value
 *
 * @param minor the FPGA minor version
 */

void smile_fee_set_hk_fpga_major_version(uint8_t major)
{
	smile_fee->hk_reg_35 &= ~(0xFUL << 8);
	smile_fee->hk_reg_35 |=  (0xFUL & ((uint32_t) major)) << 8;
}
#endif /* FEE_SIM */


/**
 * @brief get the board id the FPGA is housed on
 *
 * @returns the FPGA board id
 */

uint16_t smile_fee_get_hk_board_id(void)
{
	return (uint16_t) ((smile_fee->hk_reg_35 >> 20) & 0xFUL);
}


#ifdef FEE_SIM

/**
 * @brief set the board id the FPGA is housed on
 *
 * @param id the FPGA board id
 */

void smile_fee_set_hk_board_id(uint16_t id)
{
	smile_fee->hk_reg_35 &= ~(0x1FFUL << 20);
	smile_fee->hk_reg_35 |=  (0x1FFUL & ((uint32_t) id)) << 20;
}
#endif /* FEE_SIM */


/**
 * @brief sync configuration register 0
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_0(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_0,
				      &smile_fee->cfg_reg_0, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_0,
				      &smile_fee->cfg_reg_0, 4);

	return -1;
}


/**
 * @brief sync configuration register 1
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_1(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_1,
				      &smile_fee->cfg_reg_1, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_1,
				      &smile_fee->cfg_reg_1, 4);

	return -1;
}


/**
 * @brief sync configuration register 2
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_2(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_2,
				      &smile_fee->cfg_reg_2, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_2,
				      &smile_fee->cfg_reg_2, 4);

	return -1;
}


/**
 * @brief sync configuration register 3
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_3(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_3,
				      &smile_fee->cfg_reg_3, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_3,
				      &smile_fee->cfg_reg_3, 4);

	return -1;
}


/**
 * @brief sync configuration register 4
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_4(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_4,
				      &smile_fee->cfg_reg_4, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_4,
				      &smile_fee->cfg_reg_4, 4);

	return -1;
}


/**
 * @brief sync configuration register 5
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_5(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_5,
				      &smile_fee->cfg_reg_5, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_5,
				      &smile_fee->cfg_reg_5, 4);

	return -1;
}


/**
 * @brief sync configuration register 18
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_18(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_18,
				      &smile_fee->cfg_reg_18, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_18,
				      &smile_fee->cfg_reg_18, 4);

	return -1;
}


/**
 * @brief sync configuration register 19
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_19(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_19,
				      &smile_fee->cfg_reg_19, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_19,
				      &smile_fee->cfg_reg_19, 4);

	return -1;
}


/**
 * @brief sync configuration register 20
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_20(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_20,
				      &smile_fee->cfg_reg_20, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_20,
				      &smile_fee->cfg_reg_20, 4);

	return -1;
}


/**
 * @brief sync configuration register 21
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_21(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_21,
				      &smile_fee->cfg_reg_21, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_21,
				      &smile_fee->cfg_reg_21, 4);

	return -1;
}


/**
 * @brief sync configuration register 22
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_22(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_22,
				      &smile_fee->cfg_reg_22, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_22,
				      &smile_fee->cfg_reg_22, 4);

	return -1;
}


/**
 * @brief sync configuration register 23
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_23(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_23,
				      &smile_fee->cfg_reg_23, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_23,
				      &smile_fee->cfg_reg_23, 4);

	return -1;
}


/**
 * @brief sync configuration register 24
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_24(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_24,
				      &smile_fee->cfg_reg_24, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_24,
				      &smile_fee->cfg_reg_24, 4);

	return -1;
}


/**
 * @brief sync configuration register 25
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */
int smile_fee_sync_cfg_reg_25(enum sync_direction dir)
{
	if (dir == FEE2DPU)
		return smile_fee_sync(fee_read_cmd_cfg_reg_25,
				      &smile_fee->cfg_reg_25, 0);

	if (dir == DPU2FEE)
		return smile_fee_sync(fee_write_cmd_cfg_reg_25,
				      &smile_fee->cfg_reg_25, 4);

	return -1;
}


/**
 * @brief sync register containing vstart
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_vstart(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_0(dir);
}


/**
 * @brief sync register containing vend
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_vend(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_0(dir);
}


/**
 * @brief sync register containing charge injection width
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_charge_injection_width(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_1(dir);
}

/**
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_charge_injection_gap(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_1(dir);
}


/**
 * @brief sync the duration of a parallel overlap period (TOI)
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_parallel_toi_period(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_2(dir);
}

/**
 * @brief sync the extra parallel clock overlap
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_parallel_clk_overlap(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_2(dir);
}


/**
 * @brief sync CCD read-out
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd_readout(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_2(dir);
}


/**
 * @brief sync the amount of lines to be dumped after readout
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_n_final_dump(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_3(dir);
}


/**
 * @brief sync the amount of serial register transfers
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_h_end(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_3(dir);
}


/**
 * @brief sync charge injection mode
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_charge_injection(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_3(dir);
}


/**
 * @brief sync parallel clock generation
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_tri_level_clk(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_3(dir);
}


/**
 * @brief sync image clock direction
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_img_clk_dir(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_3(dir);
}


/**
 * @brief sync serial clock direction
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_clk_dir(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_3(dir);
}


/**
 * @brief sync packet size
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_packet_size(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_4(dir);
}


/**
 * @brief sync the integration period
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_int_period(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_4(dir);
}


/**
 * @brief sync internal mode sync
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_sync_sel(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_5(dir);
}


/**
 * @brief sync the readout node(s)
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_readout_nodes(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_5(dir);
}


/**
 * @brief sync digitise mode
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_digitise(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_5(dir);
}


/**
 * @brief sync correction bypass
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_correction_bypass(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_5(dir);
}


/**
 * @brief sync dg enable
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_dg_en(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_5(dir);
}



/**
 * @brief sync vod_config
 * @note no description available in register map document
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_vod_config(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_18(dir);
}


/**
 * @brief sync ccd1_vrd_config
 * @note no description available in register map document
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd1_vrd_config(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_18(dir);
}


/**
 * @brief sync ccd2_vrd_config
 * @note no description available in register map document
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd2_vrd_config(enum sync_direction dir)
{
	int ret;

	ret = smile_fee_sync_cfg_reg_18(dir);
	if (ret)
		return ret;

	return smile_fee_sync_cfg_reg_19(dir);
}


/**
 * @brief sync ccd3_vrd_config
 * @note no description available in register map document
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd3_vrd_config(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_19(dir);
}


/**
 * @brief sync ccd4_vrd_config
 * @note no description available in register map document
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd4_vrd_config(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_19(dir);
}


/**
 * @brief sync ccd_vgd_config
 * @note no description available in register map document
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd_vgd_config(enum sync_direction dir)
{
	int ret;

	ret = smile_fee_sync_cfg_reg_19(dir);
	if (ret)
		return ret;

	return smile_fee_sync_cfg_reg_20(dir);
}


/**
 * @brief sync ccd_vog_config
 * @note no description available in register map document
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd_vog_config(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_20(dir);
}


/**
 * @brief get ccd_ig_hi_config
 * @note no description available in register map document
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd_ig_hi_config(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_20(dir);
}


/**
 * @brief sync ccd_ig_lo_config
 * @note no description available in register map document
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd_ig_lo_config(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_21(dir);
}


/**
 * @brief sync start row number
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_h_start(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_21(dir);
}


/**
 * @brief sync next mode of operation
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd_mode_config(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_21(dir);
}


/**
 * @brief sync degree of binning
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd_mode2_config(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_21(dir);
}


/**
 * @brief sync event detection execution
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_event_detection(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_21(dir);
}


/**
 * @brief sync error flags clear
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_clear_error_flag(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_21(dir);
}


/**
 * @brief sync ccd2 E single pixel threshold
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd2_e_pix_treshold(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_22(dir);
}


/**
 * @brief sync ccd2 F single pixel threshold
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd2_f_pix_treshold(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_22(dir);
}


/**
 * @brief sync ccd4 E single pixel threshold
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd4_e_pix_treshold(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_23(dir);
}


/**
 * @brief sync ccd4 F single pixel threshold
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_ccd4_f_pix_treshold(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_23(dir);
}


/**
 * @brief sync pix offset
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_pix_offset(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_24(dir);
}


/**
 * @brief sync event pkt limit
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_event_pkt_limit(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_24(dir);
}


/**
 * @brief sync execute op flag
 *
 * @param dir the syncronisation direction
 *
 * @returns 0 on success, otherwise error
 */

int smile_fee_sync_execute_op(enum sync_direction dir)
{
	return smile_fee_sync_cfg_reg_25(dir);
}


/**
 * @brief sync ALL HK registers
 *
 * @returns 0 on success, otherwise error occured in at least one transaction
 *
 * @note HK is read only, so this only syncs FEE->DPU
 */

int smile_fee_sync_hk_regs(void)
{
	int err = 0;

	err |= smile_fee_sync(fee_read_cmd_hk_reg_3, &smile_fee->hk_reg_3, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_4, &smile_fee->hk_reg_4, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_5, &smile_fee->hk_reg_5, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_6, &smile_fee->hk_reg_6, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_7, &smile_fee->hk_reg_7, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_11, &smile_fee->hk_reg_11, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_12, &smile_fee->hk_reg_12, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_13, &smile_fee->hk_reg_13, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_14, &smile_fee->hk_reg_14, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_15, &smile_fee->hk_reg_15, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_16, &smile_fee->hk_reg_16, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_17, &smile_fee->hk_reg_17, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_18, &smile_fee->hk_reg_18, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_19, &smile_fee->hk_reg_19, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_20, &smile_fee->hk_reg_20, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_21, &smile_fee->hk_reg_21, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_22, &smile_fee->hk_reg_22, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_23, &smile_fee->hk_reg_23, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_27, &smile_fee->hk_reg_27, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_28, &smile_fee->hk_reg_28, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_29, &smile_fee->hk_reg_29, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_30, &smile_fee->hk_reg_30, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_31, &smile_fee->hk_reg_31, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_32, &smile_fee->hk_reg_32, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_33, &smile_fee->hk_reg_33, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_34, &smile_fee->hk_reg_34, 0);
	err |= smile_fee_sync(fee_read_cmd_hk_reg_35, &smile_fee->hk_reg_35, 0);

	return err;
}



/**
 * @brief initialise the smile_fee control library
 *
 * @param fee_mirror the desired FEE mirror, set NULL to allocate it for you
 */

void smile_fee_ctrl_init(struct smile_fee_mirror *fee_mirror)
{
	if (!fee_mirror)
		smile_fee = (struct smile_fee_mirror *)
				malloc(sizeof(struct smile_fee_mirror));
	else
		smile_fee = fee_mirror;

	if (!smile_fee) {
		printf("Error allocating memory for the SMILE_FEE mirror\n");
		return;
	}

	bzero(smile_fee, sizeof(struct smile_fee_mirror));
}
