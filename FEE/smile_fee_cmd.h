/**
 * @file   smile_fee_cmd.h
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
 */

#ifndef SMILE_FEE_CMD_H
#define SMILE_FEE_CMD_H

#include <stdint.h>

/* FEE RW registers (SMILE-MSSL-PL-Register_map_v0.15_Draft) */

#define FEE_CFG_REG_0		0x00000000UL
#define FEE_CFG_REG_1		0x00000004UL
#define FEE_CFG_REG_2		0x00000008UL
#define FEE_CFG_REG_3		0x0000000CUL
#define FEE_CFG_REG_4		0x00000010UL
#define FEE_CFG_REG_5		0x00000014UL
#define FEE_CFG_REG_6		0x00000018UL	/* unused */
#define FEE_CFG_REG_7		0x0000001CUL	/* unused */
#define FEE_CFG_REG_8		0x00000020UL	/* unused */
#define FEE_CFG_REG_9		0x00000024UL	/* unused */
#define FEE_CFG_REG_10		0x00000028UL	/* unused */
#define FEE_CFG_REG_11		0x0000002CUL	/* unused */
#define FEE_CFG_REG_12		0x00000030UL	/* unused */
#define FEE_CFG_REG_13		0x00000034UL	/* unused */
#define FEE_CFG_REG_14		0x00000038UL	/* unused */
#define FEE_CFG_REG_15		0x0000003CUL	/* unused */
#define FEE_CFG_REG_16		0x00000040UL	/* unused */
#define FEE_CFG_REG_17		0x00000044UL	/* unused */
#define FEE_CFG_REG_18		0x00000048UL
#define FEE_CFG_REG_19		0x0000004CUL
#define FEE_CFG_REG_20		0x00000050UL
#define FEE_CFG_REG_21		0x00000054UL
#define FEE_CFG_REG_22		0x00000058UL
#define FEE_CFG_REG_23		0x0000005CUL
#define FEE_CFG_REG_24		0x00000060UL
#define FEE_CFG_REG_25		0x00000064UL


/* FEE  RO registers (SMILE-MSSL-PL-Register_map_v0.15_Draft) */

#define FEE_HK_REG_0		0x00000700	/* reserved */
#define FEE_HK_REG_1		0x00000704	/* reserved */
#define FEE_HK_REG_2		0x00000708	/* reserved */
#define FEE_HK_REG_3		0x0000070C
#define FEE_HK_REG_4		0x00000710
#define FEE_HK_REG_5		0x00000714
#define FEE_HK_REG_6		0x00000718
#define FEE_HK_REG_7		0x0000071C
#define FEE_HK_REG_8		0x00000720	/* reserved */
#define FEE_HK_REG_9		0x00000724	/* reserved */
#define FEE_HK_REG_10		0x00000728	/* reserved */
#define FEE_HK_REG_11		0x0000072C
#define FEE_HK_REG_12		0x00000730
#define FEE_HK_REG_13		0x00000734
#define FEE_HK_REG_14		0x00000738
#define FEE_HK_REG_15		0x0000073C
#define FEE_HK_REG_16		0x00000740
#define FEE_HK_REG_17		0x00000744
#define FEE_HK_REG_18		0x00000748
#define FEE_HK_REG_19		0x0000074C
#define FEE_HK_REG_20		0x00000750
#define FEE_HK_REG_21		0x00000754
#define FEE_HK_REG_22		0x00000758
#define FEE_HK_REG_23		0x0000075C
#define FEE_HK_REG_24		0x00000760	/* reserved */
#define FEE_HK_REG_25		0x00000764	/* reserved */
#define FEE_HK_REG_26		0x00000768	/* reserved */
#define FEE_HK_REG_27		0x0000076C
#define FEE_HK_REG_28		0x00000770
#define FEE_HK_REG_29		0x00000774
#define FEE_HK_REG_30		0x00000778
#define FEE_HK_REG_31		0x0000077C
#define FEE_HK_REG_32		0x00000780
#define FEE_HK_REG_33		0x00000784
#define FEE_HK_REG_34		0x00000788
#define FEE_HK_REG_35		0x0000078C



int fee_read_cmd_register(uint16_t trans_id, uint8_t *cmd, uint32_t addr);
int fee_write_cmd_register(uint16_t trans_id, uint8_t *cmd, uint32_t addr);

int fee_write_cmd_data(uint16_t trans_id, uint8_t *cmd,
		       uint32_t addr, uint32_t size);
int fee_read_cmd_data(uint16_t trans_id, uint8_t *cmd,
		      uint32_t addr, uint32_t size);



/* FEE configuration register read accessors */
int fee_read_cmd_cfg_reg_0(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_1(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_2(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_3(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_4(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_5(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_18(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_19(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_20(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_21(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_22(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_23(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_24(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_cfg_reg_25(uint16_t trans_id, uint8_t *cmd);


/* FEE configuration register write accessors */
int fee_write_cmd_cfg_reg_0(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_1(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_2(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_3(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_4(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_5(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_18(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_19(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_20(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_21(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_22(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_23(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_24(uint16_t trans_id, uint8_t *cmd);
int fee_write_cmd_cfg_reg_25(uint16_t trans_id, uint8_t *cmd);


/* FEE housekeeping register read accessors */
int fee_read_cmd_hk_reg_3(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_4(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_5(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_6(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_7(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_11(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_12(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_13(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_14(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_15(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_16(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_17(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_18(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_19(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_20(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_21(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_22(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_23(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_27(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_28(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_29(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_30(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_31(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_32(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_33(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_34(uint16_t trans_id, uint8_t *cmd);
int fee_read_cmd_hk_reg_35(uint16_t trans_id, uint8_t *cmd);




#endif /* SMILE_FEE_CMD_H */
