/**
 * @file   fee_sim.h
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @date   2022
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
 * @brief SMILE FEE simulator
 *
 */

#ifndef SIM_H
#define SIM_H


#include <pthread.h>


struct sim_net_cfg {

	int socket;

	int n_fd;
	fd_set set;

	pthread_t thread_accept;
	pthread_t thread_poll;

	int  raw;	/* if set, use raw bytes on user ports,
			 * otherwise we expect gresb packet format
			 */
};

void fee_sim_main(struct sim_net_cfg *cfg);
void fee_send_non_rmap(struct sim_net_cfg *cfg, uint8_t *buf, size_t n);

#endif /* SIM_H */
