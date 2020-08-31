/**
 * @file   smile_fee_cfg.h
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
 * @brief SMILE FEE usage demonstrator configuration
 */

#ifndef _SMILE_FEE_CFG_H_
#define _SMILE_FEE_CFG_H_

#include <stdint.h>


#define DPU_ADDR	0x50
#define FEE_ADDR	0x51

#define DPATH	{1}
#define RPATH	{0, 0, 0, 33}

#define DPATH_LEN	0
#define RPATH_LEN	0

/* default FEE destkey */
#define FEE_DEST_KEY	0xD1


/* set header size maximum so we can fit the largest possible rmap commands
 * given the hardware limit (HEADERLEN is 8 bits wide, see GR712RC UM, p. 112)
 */
#define HDR_SIZE	0xFF



#endif /* _SMILE_FEE_CFG_H_ */
