/**
 * @file   gresb.h
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
 * @brief GRESB protocol interface
 */

#ifndef GRESB_H
#define GRESB_H

#include <stdint.h>


/**
 * highest number of virtual link ports on the GRESB
 * total 6, index starts at 0
 * base port address is 3000, two ports per link: even is TX, odd is RX
 *
 * @see GRESB-UM v1.5.14, p. 5
 */

#define GRESB_VLINK_MAX		5
#define GRESB_VLINK_PORT_BASE	3000
#define GRESB_VLINK_TX(link)	(GRESB_VLINK_PORT_BASE + (2 * (link)))
#define GRESB_VLINK_RX(link)	(GRESB_VLINK_PORT_BASE + (2 * (link) + 1))


/**
 * the maximum packet size exchangeable with the GRESB
 *
 * @see GRESB-UM v1.5.14, pp. 5
 */

#define GRESB_SNIFF_HDR_SIZE	13
#define GRESB_SPW_DATA_MAX_SIZE	0x8000000
#define GRESB_PKT_SIZE_MAX	(GRESB_SPW_DATA_MAX_SIZE + GRESB_SNIFF_HDR_SIZE)


/**
 * host to GRESB protocol ids
 *
 * @see GRESB-UM v1.5.14, pp. 8
 */

#define GRESB_FROM_HOST_DATA		0
#define GRESB_FROM_HOST_SET_CFG		1
#define GRESB_FROM_HOST_GET_CFG		2
#define GRESB_FROM_HOST_SEND_TIME	3


__extension__
struct host_to_gresb_pkt {

	union {
		struct {
			uint8_t protocol;
			uint8_t size[3];
		};
		uint32_t hdr;
	};

	uint8_t data[];
}__attribute__((packed));


__extension__
struct gresb_to_host_pkt {

	union {
		struct {
			uint8_t reserved:6;
			uint8_t truncated:1;
			uint8_t eeop:1;
			uint8_t size[3];
		};
		uint32_t hdr;
	};

	uint8_t data[];
}__attribute__((packed));



uint8_t *gresb_create_host_data_pkt(const uint8_t *data, uint32_t len);
void gresb_destroy_host_data_pkt(struct host_to_gresb_pkt *pkt);
size_t gresb_get_host_data_pkt_size(uint8_t *buf);

const uint8_t *gresb_get_spw_data(const uint8_t *buf);
size_t gresb_get_spw_data_size(uint8_t *buf);

uint8_t gresb_get_spw_pkt_truncated(uint8_t *buf);
uint8_t gresb_get_spw_pkt_eeop(uint8_t *buf);


int gresb_get_virtual_link_tx_port(unsigned int link);
int gresb_get_virtual_link_rx_port(unsigned int link);

#endif /* GRESB_H */
