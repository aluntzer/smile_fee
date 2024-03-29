/**
 * @file   smile_fee_ctrl.h
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
 * @brief SMILE FEE control interface
 */
#ifndef _SMILE_FEE_CTRL_H_
#define _SMILE_FEE_CTRL_H_

#include <stddef.h>
#include <stdint.h>

__extension__
struct smile_fee_mirror {

	/* FEE RW registers (SMILE-MSSL-PL-Register_map_v0.38) */
	/* (includes unused) */
	uint32_t cfg_reg_0;
	uint32_t cfg_reg_1;
	uint32_t cfg_reg_2;
	uint32_t cfg_reg_3;
	uint32_t cfg_reg_4;
	uint32_t cfg_reg_5;
	uint32_t cfg_reg_6;
	uint32_t cfg_reg_7;
	uint32_t cfg_reg_8;
	uint32_t cfg_reg_9;
	uint32_t cfg_reg_10;    /* unused */
	uint32_t cfg_reg_11;    /* unused */
	uint32_t cfg_reg_12;    /* unused */
	uint32_t cfg_reg_13;    /* unused */
	uint32_t cfg_reg_14;
	uint32_t cfg_reg_15;
	uint32_t cfg_reg_16;
	uint32_t cfg_reg_17;
	uint32_t cfg_reg_18;
	uint32_t cfg_reg_19;
	uint32_t cfg_reg_20;
	uint32_t cfg_reg_21;
	uint32_t cfg_reg_22;
	uint32_t cfg_reg_23;
	uint32_t cfg_reg_24;
	uint32_t cfg_reg_25;

	/* FEE  RO registers (SMILE-MSSL-PL-Register_map_v0.38) */

	uint32_t unused[422];	/* gap */

	uint32_t hk_reg_0;	/* reserved */
	uint32_t hk_reg_1;	/* reserved */
	uint32_t hk_reg_2;	/* reserved */
	uint32_t hk_reg_3;	/* reserved */
	uint32_t hk_reg_4;
	uint32_t hk_reg_5;
	uint32_t hk_reg_6;
	uint32_t hk_reg_7;
	uint32_t hk_reg_8;
	uint32_t hk_reg_9;
	uint32_t hk_reg_10;
	uint32_t hk_reg_11;
	uint32_t hk_reg_12;
	uint32_t hk_reg_13;
	uint32_t hk_reg_14;
	uint32_t hk_reg_15;
	uint32_t hk_reg_16;
	uint32_t hk_reg_17;
	uint32_t hk_reg_18;
	uint32_t hk_reg_19;
	uint32_t hk_reg_20;
	uint32_t hk_reg_21;
	uint32_t hk_reg_22;
	uint32_t hk_reg_23;
	uint32_t hk_reg_24;	/* reserved */
	uint32_t hk_reg_25;	/* reserved */
	uint32_t hk_reg_26;	/* reserved */
	uint32_t hk_reg_27;	/* reserved */
	uint32_t hk_reg_28;	/* reserved */
	uint32_t hk_reg_29;	/* reserved */
	uint32_t hk_reg_30;	/* reserved */
	uint32_t hk_reg_31;	/* reserved */
	uint32_t hk_reg_32;
	uint32_t hk_reg_33;
	uint32_t hk_reg_34;
	uint32_t hk_reg_35;
	uint32_t hk_reg_36;
	uint32_t hk_reg_37;

	/* arbitaray ram area */
	uint8_t *sram;
};


void smile_fee_print_mirrorval(void);

/* register sync */

/**
 * @note FEE2DPU is for "read" commands
 *	 DPU2FEE is for "write" commands
 */
enum sync_direction {FEE2DPU, DPU2FEE};

/* whole registers */
int smile_fee_sync_cfg_reg_0(enum sync_direction dir);
int smile_fee_sync_cfg_reg_1(enum sync_direction dir);
int smile_fee_sync_cfg_reg_2(enum sync_direction dir);
int smile_fee_sync_cfg_reg_3(enum sync_direction dir);
int smile_fee_sync_cfg_reg_4(enum sync_direction dir);
int smile_fee_sync_cfg_reg_5(enum sync_direction dir);
int smile_fee_sync_cfg_reg_6(enum sync_direction dir);
int smile_fee_sync_cfg_reg_7(enum sync_direction dir);
int smile_fee_sync_cfg_reg_8(enum sync_direction dir);
int smile_fee_sync_cfg_reg_9(enum sync_direction dir);
int smile_fee_sync_cfg_reg_14(enum sync_direction dir);
int smile_fee_sync_cfg_reg_15(enum sync_direction dir);
int smile_fee_sync_cfg_reg_16(enum sync_direction dir);
int smile_fee_sync_cfg_reg_17(enum sync_direction dir);
int smile_fee_sync_cfg_reg_18(enum sync_direction dir);
int smile_fee_sync_cfg_reg_19(enum sync_direction dir);
int smile_fee_sync_cfg_reg_20(enum sync_direction dir);
int smile_fee_sync_cfg_reg_21(enum sync_direction dir);
int smile_fee_sync_cfg_reg_22(enum sync_direction dir);
int smile_fee_sync_cfg_reg_23(enum sync_direction dir);
int smile_fee_sync_cfg_reg_24(enum sync_direction dir);
int smile_fee_sync_cfg_reg_25(enum sync_direction dir);

int smile_fee_sync_hk_regs(void);


/* values contained in registers */
int smile_fee_sync_vstart(enum sync_direction dir);
int smile_fee_sync_vend(enum sync_direction dir);
int smile_fee_sync_charge_injection_width(enum sync_direction dir);
int smile_fee_sync_charge_injection_gap(enum sync_direction dir);
int smile_fee_sync_parallel_toi_period(enum sync_direction dir);
int smile_fee_sync_parallel_clk_overlap(enum sync_direction dir);
int smile_fee_sync_ccd_readout(enum sync_direction dir);
int smile_fee_sync_n_final_dump(enum sync_direction dir);
int smile_fee_sync_h_end(enum sync_direction dir);
int smile_fee_sync_charge_injection(enum sync_direction dir);
int smile_fee_sync_tri_level_clk(enum sync_direction dir);
int smile_fee_sync_img_clk_dir(enum sync_direction dir);
int smile_fee_sync_reg_clk_dir(enum sync_direction dir);
int smile_fee_sync_packet_size(enum sync_direction dir);
int smile_fee_sync_cdsclp_hi(enum sync_direction dir);
int smile_fee_sync_adc_pwrdn_en(enum sync_direction dir);



int smile_fee_sync_trap_pumping_dwell_term(enum sync_direction dir);
int smile_fee_sync_sync_sel(enum sync_direction dir);
int smile_fee_sync_sel_pwr_syncl(enum sync_direction dir);
int smile_fee_sync_use_pwr_sync(enum sync_direction dir);
int smile_fee_sync_readout_node_sel(enum sync_direction dir);
int smile_fee_sync_digitise_en(enum sync_direction dir);
int smile_fee_sync_correction_bypass(enum sync_direction dir);
int smile_fee_sync_dg_en(enum sync_direction dir);
int smile_fee_sync_readout_pause_counter(enum sync_direction dir);
int smile_fee_sync_trap_pumping_shuffle_counter(enum sync_direction dir);
int smile_fee_sync_correction_type(enum sync_direction dir);
int smile_fee_sync_edu_wandering_mask_en(enum sync_direction dir);
int smile_fee_sync_pix_offset(enum sync_direction dir);
int smile_fee_sync_int_sync_period(enum sync_direction dir);
int smile_fee_sync_cdsclp_lo(enum sync_direction dir);
int smile_fee_sync_rowclp_hi(enum sync_direction dir);
int smile_fee_sync_ccd2_vod_config(enum sync_direction dir);
int smile_fee_sync_ccd4_vod_config(enum sync_direction dir);
int smile_fee_sync_ccd2_vrd_config(enum sync_direction dir);
int smile_fee_sync_ccd4_vrd_config(enum sync_direction dir);
int smile_fee_sync_ccd2_vgd_config(enum sync_direction dir);
int smile_fee_sync_ccd4_vgd_config(enum sync_direction dir);
int smile_fee_sync_ccd_vog_config(enum sync_direction dir);
int smile_fee_sync_vod_config(enum sync_direction dir);
int smile_fee_sync_rowclp_lo(enum sync_direction dir);
int smile_fee_sync_h_start(enum sync_direction dir);
int smile_fee_sync_ccd_mode_config(enum sync_direction dir);
int smile_fee_sync_ccd_mode2_config(enum sync_direction dir);
int smile_fee_sync_event_detection(enum sync_direction dir);
int smile_fee_sync_clear_error_flag(enum sync_direction dir);
int smile_fee_sync_ccd2_e_pix_threshold(enum sync_direction dir);
int smile_fee_sync_ccd2_f_pix_threshold(enum sync_direction dir);
int smile_fee_sync_ccd4_e_pix_threshold(enum sync_direction dir);
int smile_fee_sync_ccd4_f_pix_threshold(enum sync_direction dir);
int smile_fee_sync_event_pkt_limit(enum sync_direction dir);
int smile_fee_sync_execute_op(enum sync_direction dir);
int smile_fee_sync_full_sun_pix_threshold(enum sync_direction dir);


/* SRAM */
int smile_fee_sync_mirror_to_sram(uint32_t addr, uint32_t size, uint32_t mtu);
int smile_fee_sync_sram_to_mirror(uint32_t addr, uint32_t size, uint32_t mtu);



/* read SMILE_FEE register mirror */
uint16_t smile_fee_get_vstart(void);
uint16_t smile_fee_get_vend(void);
uint16_t smile_fee_get_charge_injection_width(void);
uint16_t smile_fee_get_charge_injection_gap(void);
uint16_t smile_fee_get_parallel_toi_period(void);
uint16_t smile_fee_get_parallel_clk_overlap(void);
uint32_t smile_fee_get_ccd_readout(uint32_t ccd_id);
uint16_t smile_fee_get_n_final_dump(void);
uint16_t smile_fee_get_h_end(void);
uint32_t smile_fee_get_charge_injection(void);
uint32_t smile_fee_get_tri_level_clk(void);
uint32_t smile_fee_get_img_clk_dir(void);
uint32_t smile_fee_get_reg_clk_dir(void);
uint32_t smile_fee_get_clk_dir(void);
uint16_t smile_fee_get_packet_size(void);
uint32_t smile_fee_get_trap_pumping_dwell_term(void);
uint32_t smile_fee_get_sync_sel(void);
uint32_t smile_fee_get_sel_pwr_sync(void);
uint32_t smile_fee_get_use_pwr_sync(void);
uint16_t smile_fee_get_readout_node_sel(void);
uint16_t smile_fee_get_readout_pause_counter(void);
uint32_t smile_fee_get_digitise_en(void);
uint32_t smile_fee_get_correction_bypass(void);
uint32_t smile_fee_get_dg_en(void);
uint16_t smile_fee_get_trap_pumping_shuffle_counter(void);
uint32_t smile_fee_get_correction_type(void);
uint32_t smile_fee_get_edu_wandering_mask_en(void);
int16_t smile_fee_get_pix_offset(void);
uint32_t smile_fee_get_int_sync_period(void);
uint16_t smile_fee_get_cdsclp_hi(void);
uint16_t smile_fee_get_cdsclp_lo(void);
uint32_t smile_fee_get_ccd2_vod_config(void);
uint32_t smile_fee_get_ccd4_vod_config(void);
uint32_t smile_fee_get_ccd2_vrd_config(void);
uint32_t smile_fee_get_ccd4_vrd_config(void);
uint32_t smile_fee_get_ccd2_vgd_config(void);
uint32_t smile_fee_get_ccd4_vgd_config(void);
uint32_t smile_fee_get_ccd_vog_config(void);
uint16_t smile_fee_get_rowclp_lo(void);
uint16_t smile_fee_get_h_start(void);
uint8_t smile_fee_get_ccd_mode_config(void);
uint8_t smile_fee_get_ccd_mode2_config(void);
uint32_t smile_fee_get_event_detection(void);
uint32_t smile_fee_get_clear_error_flag(void);
uint16_t smile_fee_get_ccd2_e_pix_threshold(void);
uint16_t smile_fee_get_ccd2_f_pix_threshold(void);
uint16_t smile_fee_get_ccd4_e_pix_threshold(void);
uint16_t smile_fee_get_ccd4_f_pix_threshold(void);
uint32_t smile_fee_get_event_pkt_limit(void);
uint32_t smile_fee_get_execute_op(void);
uint16_t smile_fee_get_full_sun_pix_threshold(void);



/* write SMILE_FEE register mirror */
void smile_fee_set_vstart(uint16_t vstart);
void smile_fee_set_vend(uint16_t vend);
void smile_fee_set_charge_injection_width(uint16_t width);
void smile_fee_set_charge_injection_gap(uint16_t width);
void smile_fee_set_parallel_toi_period(uint16_t period);
void smile_fee_set_parallel_clk_overlap(uint16_t overlap);
void smile_fee_set_ccd_readout(uint32_t ccd_id, uint32_t status);
void smile_fee_set_n_final_dump(uint16_t lines);
void smile_fee_set_h_end(uint16_t transfers);
void smile_fee_set_charge_injection(uint32_t mode);
void smile_fee_set_tri_level_clk(uint32_t mode);
void smile_fee_set_img_clk_dir(uint32_t mode);
void smile_fee_set_reg_clk_dir(uint32_t mode);
void smile_fee_set_packet_size(uint16_t pkt_size);
void smile_fee_set_trap_pumping_dwell_term(uint32_t dwell);
void smile_fee_set_readout_node_sel(uint32_t nodes);
void smile_fee_set_readout_pause_counter(uint16_t readout_pause);
void smile_fee_set_digitise_en(uint32_t mode);
void smile_fee_set_sync_sel(uint32_t mode);
void smile_fee_set_sel_pwr_sync(uint32_t mode);
void smile_fee_set_use_pwr_sync(uint32_t mode);
void smile_fee_set_correction_bypass(uint32_t mode);
void smile_fee_set_dg_en(uint32_t mode);
void smile_fee_set_trap_pumping_shuffle_counter(uint16_t shuffle_counter);
void smile_fee_set_correction_type(uint32_t mode);
void smile_fee_set_edu_wandering_mask_en(uint32_t mode);
void smile_fee_set_pix_offset(int16_t offset);
void smile_fee_set_int_sync_period(uint32_t period);
void smile_fee_set_cdsclp_lo(uint16_t cdsclp);
void smile_fee_set_ccd2_vod_config(uint32_t vod);
void smile_fee_set_ccd4_vod_config(uint32_t vod);
void smile_fee_set_ccd2_vrd_config(uint32_t vrd);
void smile_fee_set_ccd4_vrd_config(uint32_t vrd);
void smile_fee_set_ccd2_vgd_config(uint32_t vgd);
void smile_fee_set_ccd4_vgd_config(uint32_t vgd);
void smile_fee_set_ccd_vog_config(uint32_t vog);
void smile_fee_set_rowclp_lo(uint16_t rowclp_lo);
void smile_fee_set_h_start(uint16_t row);
void smile_fee_set_ccd_mode_config(uint8_t mode);
void smile_fee_set_ccd_mode2_config(uint8_t mode);
void smile_fee_set_event_detection(uint32_t mode);
void smile_fee_set_clear_error_flag(uint32_t mode);
void smile_fee_set_ccd2_e_pix_threshold(uint16_t threshold);
void smile_fee_set_ccd2_f_pix_threshold(uint16_t threshold);
void smile_fee_set_ccd4_e_pix_threshold(uint16_t threshold);
void smile_fee_set_ccd4_f_pix_threshold(uint16_t threshold);
void smile_fee_set_event_pkt_limit(uint32_t pkt_limit);
void smile_fee_set_execute_op(uint32_t mode);
void smile_fee_set_full_sun_pix_threshold(uint16_t threshold);


/* read SMILE_FEE housekeeping register mirror */
uint16_t smile_fee_get_hk_ccd2_ts_a(void);
uint16_t smile_fee_get_hk_ccd4_ts_b(void);
uint16_t smile_fee_get_hk_prt1(void);
uint16_t smile_fee_get_hk_prt2(void);
uint16_t smile_fee_get_hk_prt3(void);
uint16_t smile_fee_get_hk_prt4(void);
uint16_t smile_fee_get_hk_prt5(void);
uint16_t smile_fee_get_hk_ccd4_vod_mon_e(void);
uint16_t smile_fee_get_hk_ccd4_vog_mon(void);
uint16_t smile_fee_get_hk_ccd4_vrd_mon_e(void);
uint16_t smile_fee_get_hk_ccd2_vod_mon_e(void);
uint16_t smile_fee_get_hk_ccd2_vog_mon(void);
uint16_t smile_fee_get_hk_ccd2_vrd_mon_e(void);
uint16_t smile_fee_get_hk_ccd4_vrd_mon_f(void);
uint16_t smile_fee_get_hk_ccd4_vdd_mon(void);
uint16_t smile_fee_get_hk_ccd4_vgd_mon(void);
uint16_t smile_fee_get_hk_ccd2_vrd_mon_f(void);
uint16_t smile_fee_get_hk_ccd2_vdd_mon(void);
uint16_t smile_fee_get_hk_ccd2_vgd_mon(void);
uint16_t smile_fee_get_hk_vccd(void);
uint16_t smile_fee_get_hk_vrclk_mon(void);
uint16_t smile_fee_get_hk_viclk(void);
uint16_t smile_fee_get_hk_ccd4_vod_mon_f(void);
uint16_t smile_fee_get_hk_5vb_pos_mon(void);
uint16_t smile_fee_get_hk_5vb_neg_mon(void);
uint16_t smile_fee_get_hk_3v3b_mon(void);
uint16_t smile_fee_get_hk_2v5a_mon(void);
uint16_t smile_fee_get_hk_3v3d_mon(void);
uint16_t smile_fee_get_hk_2v5d_mon(void);
uint16_t smile_fee_get_hk_1v2d_mon(void);
uint16_t smile_fee_get_hk_5vref_mon(void);
uint16_t smile_fee_get_hk_vccd_pos_raw(void);
uint16_t smile_fee_get_hk_vclk_pos_raw(void);
uint16_t smile_fee_get_hk_van1_pos_raw(void);
uint16_t smile_fee_get_hk_van3_neg_mon(void);
uint16_t smile_fee_get_hk_van2_pos_raw(void);
uint16_t smile_fee_get_hk_vdig_raw(void);
uint16_t smile_fee_get_hk_ig_hi_mon(void);
uint16_t smile_fee_get_hk_ccd2_vod_mon_f(void);
uint8_t smile_fee_get_spw_id(void);
uint8_t smile_fee_get_hk_timecode_from_spw(void);
uint8_t smile_fee_get_hk_rmap_target_status(void);
uint32_t smile_fee_get_hk_rmap_target_indicate(void);
uint32_t smile_fee_get_hk_spw_link_escape_error(void);
uint32_t smile_fee_get_hk_spw_link_credit_error(void);
uint32_t smile_fee_get_hk_spw_link_parity_error(void);
uint32_t smile_fee_get_hk_spw_link_disconnect(void);
uint32_t smile_fee_get_hk_spw_link_running(void);
uint32_t smile_fee_get_hk_frame_counter(void);
uint8_t smile_fee_get_hk_fpga_op_mode(void);
uint32_t smile_fee_get_hk_error_flag_spw_dac_on_bias_readback(void);
uint32_t smile_fee_get_hk_error_flag_dac_off_bias_readback_error(void);
uint32_t smile_fee_get_hk_error_flag_ext_sdram_edac_corr_err_error(void);
uint32_t smile_fee_get_hk_error_flag_ext_sdram_edac_uncorr_err_error(void);
uint32_t smile_fee_get_hk_error_flag_spw_link_disconnect_error(void);
uint32_t smile_fee_get_hk_error_flag_spw_link_escape_error(void);
uint32_t smile_fee_get_hk_error_flag_spw_link_credit_error(void);
uint32_t smile_fee_get_hk_error_flag_spw_link_parity_error(void);
uint8_t smile_fee_get_hk_fpga_minor_version(void);
uint8_t smile_fee_get_hk_fpga_major_version(void);
uint16_t smile_fee_get_hk_board_id(void);
uint16_t smile_fee_get_cmic_corr(void);
uint16_t smile_fee_get_hk_ccd2_e_pix_full_sun(void);
uint16_t smile_fee_get_hk_ccd2_f_pix_full_sun(void);
uint16_t smile_fee_get_hk_ccd4_e_pix_full_sun(void);
uint16_t smile_fee_get_hk_ccd4_f_pix_full_sun(void);



#ifdef FEE_SIM
/* write SMILE_FEE housekeeping register mirror (for FEE simulator) */
void smile_fee_set_hk_ccd2_ts_a(uint16_t ccd2_ts_a);
void smile_fee_set_hk_ccd4_ts_b(uint16_t ccd4_ts_b);
void smile_fee_set_hk_prt1(uint16_t prt1);
void smile_fee_set_hk_prt2(uint16_t prt2);
void smile_fee_set_hk_prt3(uint16_t prt3);
void smile_fee_set_hk_prt4(uint16_t prt4);
void smile_fee_set_hk_prt5(uint16_t prt5);
void smile_fee_set_hk_ccd4_vod_mon_e(uint16_t ccd4_vod_mon_e);
void smile_fee_set_hk_ccd4_vog_mon(uint16_t ccd4_vog_mon);
void smile_fee_set_hk_ccd4_vrd_mon_e(uint16_t ccd4_vrd_mon_e);
void smile_fee_set_hk_ccd2_vod_mon_e(uint16_t ccd2_vrd_mon_e);
void smile_fee_set_hk_ccd2_vog_mon(uint16_t ccd2_vog_mon);
void smile_fee_set_hk_ccd2_vrd_mon_e(uint16_t ccd2_vrd_mon_e);
void smile_fee_set_hk_ccd4_vrd_mon_f(uint16_t ccd4_vrd_mon_f);
void smile_fee_set_hk_ccd4_vdd_mon(uint16_t ccd4_vdd_mon);
void smile_fee_set_hk_ccd4_vgd_mon(uint16_t ccd4_vgd_mon);
void smile_fee_set_hk_ccd2_vrd_mon_f(uint16_t ccd2_vrd_mon_f);
void smile_fee_set_hk_ccd2_vdd_mon(uint16_t ccd2_vdd_mon);
void smile_fee_set_hk_ccd2_vgd_mon(uint16_t ccd2_vgd_mon);
void smile_fee_set_hk_vccd(uint16_t vccd);
void smile_fee_set_hk_vrclk_mon(uint16_t vrclk_mon);
void smile_fee_set_hk_viclk(uint16_t viclk);
void smile_fee_set_hk_ccd4_vod_mon_f(uint16_t ccd4_vod_mon_f);
void smile_fee_set_hk_5vb_pos_mon(uint16_t _5vb_pos_mon);
void smile_fee_set_hk_5vb_neg_mon(uint16_t _5vb_neg_mon);
void smile_fee_set_hk_3v3b_mon(uint16_t _3v3b_mon);
void smile_fee_set_hk_3v3d_mon(uint16_t _3v3d_mon);
void smile_fee_set_hk_2v5d_mon(uint16_t _2v5d_mon);
void smile_fee_set_hk_2v5d_mon(uint16_t _2v5d_mon);
void smile_fee_set_hk_1v2d_mon(uint16_t _1v2d_mon);
void smile_fee_set_hk_5vref_mon(uint16_t _5vref_mon);
void smile_fee_set_hk_vccd_pos_raw(uint16_t vccd_pos_raw);
void smile_fee_set_hk_vclk_pos_raw(uint16_t vclk_pos_raw);
void smile_fee_set_hk_van1_pos_raw(uint16_t van1_pos_raw);
void smile_fee_set_hk_van3_neg_mon(uint16_t van3_neg_raw);
void smile_fee_set_hk_van2_pos_raw(uint16_t van2_pos_raw);
void smile_fee_set_hk_vdig_raw(uint16_t vdig_raw);
void smile_fee_set_hk_ig_hi_mon(uint16_t ig_hi_mon);
void smile_fee_set_hk_ccd2_vod_mon_f(uint16_t ccd2_vod_mon_f);
void smile_fee_set_spw_id(uint8_t spw_id);
void smile_fee_set_hk_timecode_from_spw(uint8_t timecode_from_spw);
void smile_fee_set_hk_rmap_target_status(uint8_t rmap_target_status);
void smile_fee_set_hk_rmap_target_indicate(uint32_t rmap_target_indicate);
void smile_fee_set_hk_spw_link_escape_error(uint32_t spw_link_escape_error);
void smile_fee_set_hk_spw_link_credit_error(uint32_t spw_link_credit_error);
void smile_fee_set_hk_spw_link_parity_error(uint32_t spw_link_parity_error);
void smile_fee_set_hk_spw_link_disconnect_error(uint32_t spw_link_disconnect_error);
void smile_fee_set_hk_spw_link_running(uint32_t spw_link_running);
void smile_fee_set_hk_frame_counter(uint16_t frame_counter);
void smile_fee_set_hk_fpga_op_mode(uint8_t fpga_op_mode);
void smile_fee_set_hk_error_flag_dac_off_bias_readback_error(uint32_t error_flag_dac_off_bias_readback_error);
void smile_fee_set_hk_error_flag_spw_dac_on_bias_readback(uint32_t error_flag_spw_dac_on_bias_readback);
void smile_fee_set_hk_error_flag_spw_link_disconnect_error(uint32_t error_flag_spw_link_disconnect_error);
void smile_fee_set_hk_error_flag_ext_sdram_edac_corr_err_error(uint32_t error_flag_ext_sdram_edac_corr_err_error);
void smile_fee_set_hk_error_flag_ext_sdram_edac_uncorr_err_error(uint32_t error_flag_ext_sdram_edac_corr_err_error);
void smile_fee_set_hk_error_flag_spw_link_escape_error(uint32_t error_flag_spw_link_escape_error);
void smile_fee_set_hk_error_flag_spw_link_credit_error(uint32_t error_flag_spw_link_credit_error);
void smile_fee_set_hk_error_flag_spw_link_parity_error(uint32_t error_flag_spw_link_parity_error);
void smile_fee_set_hk_fpga_minor_version(uint8_t minor);
void smile_fee_set_hk_fpga_major_version(uint8_t major);
void smile_fee_set_hk_board_id(uint16_t id);
void smile_fee_set_cmic_corr(uint16_t corr);
void smile_fee_set_hk_ccd2_e_pix_full_sun(uint16_t ccd2_e_pix_full_sun);
void smile_fee_set_hk_ccd2_f_pix_full_sun(uint16_t ccd2_f_pix_full_sun);
void smile_fee_set_hk_ccd4_e_pix_full_sun(uint16_t ccd4_e_pix_full_sun);
void smile_fee_set_hk_ccd4_f_pix_full_sun(uint16_t ccd4_f_pix_full_sun);

#endif /* FEE_SIM */


/* SRAM */
int smile_fee_read_sram(void *buf, uint32_t addr, uint32_t size);
int smile_fee_read_sram_16(uint16_t *buf, uint32_t addr, size_t nmemb);
int smile_fee_write_sram(void *buf, uint32_t addr, uint32_t size);
int smile_fee_write_sram_16(uint16_t *buf, uint32_t addr, size_t nmemb);
int smile_fee_write_sram_32(uint32_t *buf, uint32_t addr, size_t nmemb);


/* setup */
void smile_fee_ctrl_init(struct smile_fee_mirror *fee_mirror);


#endif /* _SMILE_FEE_CTRL_H_ */
