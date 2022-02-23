/**
 * @file   gresb.c
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
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gresb.h>

/**
 * @brief set the protocol field in the header
 *
 * @pkt a host-to-gresb packet
 * @protocol the protocol to set
 *
 * @returns -1 on error
 */

static int gresb_host_pkt_set_protocol(struct host_to_gresb_pkt *pkt,
				       uint8_t protocol)
{

	if (!pkt)
		return -1;

	switch (protocol) {
	case GRESB_FROM_HOST_DATA:
	case GRESB_FROM_HOST_SET_CFG:
	case GRESB_FROM_HOST_GET_CFG:
	case GRESB_FROM_HOST_SEND_TIME:
		pkt->protocol = protocol;
		break;
	default:
		return -1;
	}

	return 0;
}


/**
 * @brief set the data size field in the header
 *
 * @pkt  a host-to-gresb packet
 * @size the data size to set
 *
 * @note size is truncated to 24 bits
 *
 * @returns -1 on error
 */

static int gresb_host_pkt_set_data_size(struct host_to_gresb_pkt *pkt,
					uint32_t size)
{
	if (!pkt)
		return -1;

	if (pkt->protocol != GRESB_FROM_HOST_DATA)
		return -1;

	/* byte order is big endian */
	pkt->size[0] = (size >> 16) & 0xff;
	pkt->size[1] = (size >>  8) & 0xff;
	pkt->size[2] = (size >>  0) & 0xff;


	return 0;
}


/**
 * @brief get the data size of a packet
 *
 * @pkt  a host-to-gresb packet
 *
 * @returns the data size
 */

static size_t gresb_host_pkt_get_data_size(struct host_to_gresb_pkt *pkt)
{
	size_t n = 0;

	if (pkt->protocol == GRESB_FROM_HOST_DATA) {

		n  = (pkt->size[0] << 16) & 0xff0000;
		n |= (pkt->size[1] <<  8) & 0x00ff00;
		n |= (pkt->size[2] <<  0) & 0x0000ff;
	}

	return n;
}

/**
 * @brief get the data size of a packet
 *
 * @pkt  a gresb-to-host packet
 *
 * @returns the data size
 */

static size_t gresb_pkt_get_data_size(struct gresb_to_host_pkt *pkt)
{
	size_t n;

	n  = (pkt->size[0] << 16) & 0xff0000;
	n |= (pkt->size[1] <<  8) & 0x00ff00;
	n |= (pkt->size[2] <<  0) & 0x0000ff;

	return n;
}

/**
 * @brief create a new host-to-gresb data packet
 *
 * @param data the data to append
 * @param len the length of the data
 *
 * @returns the packet or NULL on error
 */

uint8_t *gresb_create_host_data_pkt(const uint8_t *data, uint32_t len)
{
	struct host_to_gresb_pkt *pkt;


	pkt = malloc(sizeof(struct host_to_gresb_pkt) + len);
	if (!pkt)
		return NULL;

	gresb_host_pkt_set_protocol(pkt, GRESB_FROM_HOST_DATA);
	gresb_host_pkt_set_data_size(pkt, len);

	if (data)
		memcpy(pkt->data, data, len);

	return (uint8_t *) pkt;
}


/**
 * @brief destroy a new host-to-gresb data packet
 */

void gresb_destroy_host_data_pkt(struct host_to_gresb_pkt *pkt)
{
	free(pkt);
}


/**
 * @brief get the total size of a host-to-gresb data packet
 */

size_t gresb_get_host_data_pkt_size(uint8_t *buf)
{
	size_t n;


	if (!buf)
		return 0;

	n  = sizeof(struct host_to_gresb_pkt);
	n += gresb_host_pkt_get_data_size((struct host_to_gresb_pkt *) buf);

	return n;
}


/**
 * @brief get the SpW data of a gresb-to-host packet
 *
 * @param buf the buffer holding the packet
 *
 * @returns a reference to the data buffer in the packet
 */

const uint8_t *gresb_get_spw_data(const uint8_t *buf)
{
	struct gresb_to_host_pkt *pkt;


	pkt = (struct gresb_to_host_pkt *) buf;
	if (!pkt)
		return NULL;

	return pkt->data;
}


/**
 * @brief get the SpW data size of a gresb-to-host packet
 *
 * @param buf the buffer holding the packet
 *
 * @returns the packet data size, always 0 if argument is NULL
 */

size_t gresb_get_spw_data_size(uint8_t *buf)
{
	if (!buf)
		return 0;

	return gresb_pkt_get_data_size((struct gresb_to_host_pkt *) buf);
}


/**
 * @brief check if SpW packet contained in gresb-to-host packet was truncated
 *
 * @param buf the buffer holding the packet
 *
 * @returns 0 if not truncated or NULL, value set otherwise
 */

uint8_t gresb_get_spw_pkt_truncated(uint8_t *buf)
{
	struct gresb_to_host_pkt *pkt;


	pkt = (struct gresb_to_host_pkt *) buf;
	if (!pkt)
		return 0;

	return pkt->truncated;
}


/**
 * @brief check if SpW packet contained in gresb-to-host packet ended
 *	  ended with error end of packet (EEOP) character
 *
 * @param buf the buffer holding the packet
 *
 * @returns 0 if no error or NULL, value set otherwise
 */

uint8_t gresb_get_spw_pkt_eeop(uint8_t *buf)
{
	struct gresb_to_host_pkt *pkt;


	pkt = (struct gresb_to_host_pkt *) buf;
	if (!pkt)
		return 0;

	return pkt->eeop;
}


/**
 * @brief get a GRESB virtual link transmit network port
 *
 * @param link the desired virtual link (0-5)
 *
 * @returns the transmit network port or -1 on error
 */

int gresb_get_virtual_link_tx_port(unsigned int link)
{
	if (link > GRESB_VLINK_MAX)
		return -1;

	return GRESB_VLINK_TX(link);
}


/**
 * @brief get a GRESB virtual link receive network port
 *
 * @param link the desired virtual link (0-5)
 *
 * @returns the receive network port or -1 on error
 */

int gresb_get_virtual_link_rx_port(unsigned int link)
{
	if (link > GRESB_VLINK_MAX)
		return -1;

	return GRESB_VLINK_TX(link);
}



int test(void)
{
	size_t i;
	uint8_t *pkt;
	char *b;
	uint8_t buf[] = {0xa, 2, 3, 4, 5, 6, 7};


	pkt = gresb_create_host_data_pkt(buf, sizeof(buf));



	b = (char *) gresb_get_spw_data(pkt);

	for (i = 0; i < gresb_get_spw_data_size(pkt); i++)
		printf("%x:", b[i]);

	return 0;
}
