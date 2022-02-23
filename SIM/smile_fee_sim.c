/**
 * this is a very simple FEE <-> DPU interface demonstrator
 *
 * Note that we currently emulate the RMAP connection to the FEE by function
 * calls. The simulated FEE is very stupid, it does not really do anything...
 *
 * Implement your own rmap_tx and rmap_rx interface functions as needed, these
 * can be networked, use actual spacewire or whatever.
 *
 * WARNING: at this point, the FEE houskeeping read-out is not yet implemented
 *	    in the library
 */

#include <stddef.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/tcp.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

#include <sys/time.h>

#include <gresb.h>

#include <smile_fee.h>
#include <smile_fee_cfg.h>
#include <smile_fee_ctrl.h>
#include <smile_fee_rmap.h>
#include <fee_sim.h>

#include <rmap.h>	/* for FEE simulation */






/* whatever for now ... */
#define MAX_PAYLOAD_SIZE	4096

/* XXX include extra for RMAP headers, 128 bytes is plenty */
#undef GRSPW2_DEFAULT_MTU
#define GRSPW2_DEFAULT_MTU (MAX_PAYLOAD_SIZE + 128)


/* for our simulated fee, you can use the accessor part of the library and
 * the stuff below to to control you own FEE simulator
 */
static struct smile_fee_mirror smile_fee_mem;



#define DEFAULT_LINK 0
#define DEFAULT_PORT 1234
#define DEFAULT_ADDR "0.0.0.0"



static void rmap_sim_rx(uint8_t *pkt, struct sim_net_cfg *cfg);

/**
 * @brief a sigint handler, should we ever need it
 */

static void sigint_handler(__attribute__((unused)) int s)
{
	printf("\nCaught signal %d\n", s);
	exit(s);
}



static void set_tcp_nodelay(struct sim_net_cfg *cfg)
{
	int flag = 1;

	setsockopt(cfg->socket, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(int));
}


/**
 * @brief transmits the contents of a buffer from a socket to its peer
 */

static int send_all(int sock_fd, const unsigned char *buf, int len)
{
	int n;


	while (len) {

		n = send(sock_fd, buf, len, 0);

		if (n == -1) {
			perror("send_all");
			return len;
		}

		len -= n;
		buf += n;
	}

	return len;
}



/**
 * @brief push buffer to anyone connected
 */

static void distribute_tx(struct sim_net_cfg *cfg, uint8_t *buf, int len)
{
	int fdc;
	int ret;


	for(fdc = 0; fdc <= cfg->n_fd; fdc++) {

		if (!FD_ISSET(fdc, &cfg->set))
			continue;

		if (cfg->raw) {
			/* extract data */
			ret = send_all(fdc,
				       gresb_get_spw_data(buf),
				       gresb_get_spw_data_size(buf));
		} else {
			/* forward packet */
			ret = send_all(fdc, buf, len);
		}

		if (ret == -1) {
			perror("send");
			close(fdc);
			FD_CLR(fdc, &cfg->set);
		}


	}

}



/**
 * @brief create socket address from url
 *
 * @note url expected to be <ip>:<port>
 */

static struct sockaddr_in sockaddr_from_url(const char *url)
{
	char *str;

	struct sockaddr_in sockaddr;


	str = (char *) malloc(strlen(url) + 1);
	if (!str)  {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	strcpy(str, url);

	sockaddr.sin_addr.s_addr = inet_addr(strtok(str, ":"));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(atoi(strtok(NULL, ":")));

	free(str);

	return sockaddr;
}



/**
 * @brief bind a socket for a listening server
 *
 * @note url expected to be <ip>:<port>
 */

static int bind_server_socket(const char* url, struct sim_net_cfg *cfg)
{
	int fd;
	int endpoint;
	int optval = 1;

	struct sockaddr_in server;


	server = sockaddr_from_url(url);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		printf("Socket creation failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	FD_ZERO(&cfg->set);

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	endpoint = bind(fd, (struct sockaddr *) &server, sizeof(server));
	if (endpoint < 0) {
		close(fd);
		printf("could not bind endpoint %s: %s\n", url, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (listen(fd, 0) < 0) {
		close(fd);
		perror("listen");
		exit(EXIT_FAILURE);
	}

	printf("Listening on %s\n", url);

	return fd;
}


/**
 * @brief thread function accepting incoming connections on a server socket
 */

static void *accept_connections(void *data)
{
	int fd = 0;

	struct sim_net_cfg *cfg;

	struct sockaddr_storage client_addr;

	socklen_t client_addr_len;


	cfg = (struct sim_net_cfg *) data;

	client_addr_len = sizeof(client_addr);

	while(1) {

		fd = accept(cfg->socket, (struct sockaddr *) &client_addr,
			    &client_addr_len);

		if (fd < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		printf("New incoming connection\n");

		FD_SET(fd, &cfg->set);

		/* update maximum file descriptor number */
		if (fd > cfg->n_fd)
			cfg->n_fd = fd;
	}
}




/**
 * @brief received packet
 */

static int sim_rx(int fd, struct sim_net_cfg *cfg)
{
	ssize_t recv_bytes;
	ssize_t recv_left;
	unsigned char *recv_buffer;
	ssize_t pkt_size;

	uint8_t gresb_hdr[4];	/* host-to-gresb header is 4 bytes */


	if (cfg->raw) {

		recv_buffer = malloc(GRESB_PKT_SIZE_MAX);

		recv_bytes  = recv(fd, recv_buffer, GRESB_PKT_SIZE_MAX, 0);

	} else {

		/* try to grab a header */
		recv_bytes = recv(fd, gresb_hdr, 4, MSG_PEEK | MSG_DONTWAIT);
		if (recv_bytes < 4)
			return 0;

		/* we got at least a header, now allocate space for a host-to-gresb
		 * packet (data + header) and start receiving
		 * note the lack of basic sanity checks...
		 */
		pkt_size = gresb_get_spw_data_size(gresb_hdr) + 4;
		recv_buffer = malloc(pkt_size);


		/* pull in the whole packet */
		recv_bytes = 0;
		recv_left  = pkt_size ;
		while (recv_left) {
			ssize_t rb;
			rb = recv(fd, recv_buffer + recv_bytes, recv_left, 0);
			recv_bytes += rb;
			recv_left  -= rb;
		}

	}

	if (recv_bytes <= 0)
		goto cleanup;

#if DEBUG
	{
		printf("RX [%lu]: ", recv_bytes);
		int i;
		for (i = 0; i < recv_bytes; i++)
			printf("%02X:", recv_buffer[i]);
		printf("\n");
	}
#endif



	rmap_sim_rx((uint8_t *) gresb_get_spw_data(recv_buffer), cfg);

cleanup:
	free(recv_buffer);

	return recv_bytes;
}






/**
 * @brief thread function polling a network socket
 */

static void *poll_socket(void *data)
{
	int fd;

	fd_set r_set;

	struct timeval to;

	struct sim_net_cfg *cfg;


	if (!data)
		return NULL;

	cfg = (struct sim_net_cfg *) data;

	/* wait 10 ms in select() */
	to.tv_sec  = 0;
	to.tv_usec = 10000;

	while (1) {

		r_set = cfg->set;

		/* select ready sockets */
		if (select((cfg->n_fd + 1), &r_set, NULL, NULL, &to) <= 0) {
			usleep(100);
			continue;
		}

		for (fd = 0; fd <= cfg->n_fd; fd++) {

			if (!FD_ISSET(fd, &r_set))
				continue;

			/* clean up disconnected socket */
			if (sim_rx(fd, cfg) <= 0) {
				FD_CLR(fd, &cfg->set);
				close(fd);
			}
		}
	}

	return NULL;
}





/**
 * @brief create a complete package from header and payload data including CRC8
 *
 * @note this is a helper function to generate complete binary RMAP packet dumps
 *
 * @param blob the blob buffer; if NULL, the function returns the needed size
 *
 * @param[in]  cmd an rmap command buffer
 * @param[in]  cmd_size the size of the rmap command buffer
 * @param[in]  non_crc_bytes leading bytes in the header not path of the CRC
 * @param[in]  data a data buffer (may be NULL)
 * @param[in]  data_size the size of the data buffer (ignored if data is NULL)
 *
 * @returns the size of the blob or 0 on error
 */

int fee_sim_package(uint8_t *blob,
		    const uint8_t *cmd,  int cmd_size,
		    const uint8_t non_crc_bytes,
		    const uint8_t *data, int data_size)
{
	int n;
	int has_data_crc = 0;
	struct rmap_instruction *ri;



	if (!cmd_size) {
		blob = NULL;
		return 0;
	}


	/* allocate space for header, header crc, data, data crc */
	n = cmd_size + 1;

	ri = (struct rmap_instruction *) &cmd[non_crc_bytes + RMAP_INSTRUCTION];

	/* see if the type of command needs a data crc field at the end */
	switch (ri->cmd) {
	case RMAP_READ_ADDR_SINGLE:
	case RMAP_READ_ADDR_INC:
		has_data_crc = 1;
		n += 1;
		break;
	default:
		break;
	}


	if (data)
		n += data_size;

	if (!blob)
		return n;


	memcpy(&blob[0], cmd, cmd_size);

	blob[cmd_size] = rmap_crc8(&cmd[non_crc_bytes],
				   cmd_size - non_crc_bytes);

	if (data) {
		memcpy(&blob[cmd_size + 1], data, data_size);
		blob[cmd_size + 1 + data_size] = rmap_crc8(data, data_size);
	} else {
		/* if no data is present, data crc is 0x0 */
		if (has_data_crc)
			blob[cmd_size + 1] = 0x0;
	}


	return n;
}

/* simulate FEE receiving data */
static void rmap_sim_rx(uint8_t *pkt, struct sim_net_cfg *cfg)
{
	int i,n;

	uint8_t src;

	uint8_t *hdr;
	uint8_t *data = NULL;

	uint8_t *buf;
	uint8_t *gresb_pkt;

	int hdr_size;
	int data_size = 0;

	struct rmap_pkt *rp;




	rp = rmap_pkt_from_buffer(pkt);

	if (!rp) {
		printf("conversion error!");
		exit(0);
	}


	/* The rmap libary does not implement client mode, so we do the
	 * basics here.
	 * At the moment, we only use
	 *	RMAP_READ_ADDR_INC and
	 *	RMAP_WRITE_ADDR_INC_VERIFY_REPLY,
	 * because we only implemented the config registers, so this is pretty
	 * easy
	 */


	/* we reuse the packet, turn it into a response */
	rp->ri.cmd_resp = 0;

	/* WARNING: path addressing requires extra steps (leading zero removal),
	 * this not done because it's unused in this demo
	 */

	/* flip around logical addresses */
	src = rp->src;
	rp->src = rp->dst;
	rp->dst = src;

	/* XXX we do byte swaps everywhere if the transfer size is multiples of 4 */

	switch (rp->ri.cmd) {
		case RMAP_READ_ADDR_INC:
#ifdef DEBUG
			printf("RMAP_READ_ADDR_INC\n");
			printf("read from addr: %x, size %d \n", rp->addr, rp->data_len);
#endif
			data = malloc(rp->data_len);
			if (!data) {
				printf("error allocating buffer\n");
				exit(0);
			}

			data_size = rp->data_len;

			/* copy the simulated register map into the data payload
			 * This works because the register map in the FEE
			 * starts at address 0x0
			 */
			memcpy(data, (&((uint8_t *) &smile_fee_mem)[rp->addr]), data_size);

			if (!(data_size & 0x3)) {
				for (i = 0; i < data_size / 4; i++)
					__swab32s(&((uint32_t *) data)[i]);
			}

			break;

		case RMAP_WRITE_ADDR_INC_VERIFY_REPLY:

			/* note that we don't do verification */
#ifdef DEBUG
			printf("RMAP_WRITE_ADDR_INC_VERIFY_REPLY\n");
			printf("write to addr: %x, size %d \n", rp->addr, rp->data_len);
#endif
			/* copy the payload into the simulated register
			 * map. This works because the register map in the FEE
			 * starts at address 0x0
			 */
			memcpy((&((uint8_t *) &smile_fee_mem)[rp->addr]), rp->data, rp->data_len);

			if (!(rp->data_len & 0x3)) {
				for (i = 0; i < (int) rp->data_len / 4; i++)
					__swab32s(&((uint32_t *) &((uint8_t *) &smile_fee_mem)[rp->addr])[i]);
			}

			rp->data_len = 0; /* no data in reply */

			break;

		case RMAP_WRITE_ADDR_INC_REPLY:
#ifdef DEBUG
			printf("RMAP_WRITE_ADDR_INC_REPLY\n");
			printf("write to addr: %x, size %d \n", rp->addr, rp->data_len);
#endif
			/* copy the payload into the simulated register
			 * map. This works because the register map in the FEE
			 * starts at address 0x0
			 */
			memcpy((&((uint8_t *) &smile_fee_mem)[rp->addr]), rp->data, rp->data_len);

			if (!(rp->data_len & 0x3)) {
				for (i = 0; i < (int) rp->data_len / 4; i++)
					__swab32s(&((uint32_t *) &((uint8_t *) &smile_fee_mem)[rp->addr])[i]);
			}



			rp->data_len = 0; /* no data in reply */

			break;
		default:
			printf("rmap command code not implemented: %x\n", rp->ri.cmd);
			break;
	}

	/* convert packet to buffer */

	/* determine header size */
	hdr_size = rmap_build_hdr(rp, NULL);

	hdr = malloc(hdr_size);
	if (!hdr) {
		printf("Error allocating memory\n");
		exit(0);
	}

	rmap_build_hdr(rp, hdr);

	rmap_erase_packet(rp);


	/* determine required buffer size
	 * WARNING non-crc bytes are hardcoded to 0 since we do not use path
	 * addressing in this demo
	 */
	n = fee_sim_package(NULL, hdr, hdr_size, 0,
			    data, data_size);

	buf = malloc(n);

	if (!buf) {
		printf("Error allcating blob!\n");
		exit(0);
	}

	/* copy packet data to buffer */
	n = smile_fee_package(buf, hdr, hdr_size, 0,
			      data, data_size);


	gresb_pkt = gresb_create_host_data_pkt(buf, n);
	distribute_tx(cfg, gresb_pkt, gresb_get_host_data_pkt_size(gresb_pkt));

	gresb_destroy_host_data_pkt((struct host_to_gresb_pkt *) gresb_pkt);
	free(buf);
}


/**
 * @brief function to send non-rmap data packets going to the DPU
 */

void fee_send_non_rmap(struct sim_net_cfg *cfg, uint8_t *buf, size_t n)
{
	uint8_t *gresb_pkt;


	gresb_pkt = gresb_create_host_data_pkt(buf, n);

	distribute_tx(cfg, gresb_pkt, gresb_get_host_data_pkt_size(gresb_pkt));

	gresb_destroy_host_data_pkt((struct host_to_gresb_pkt *) gresb_pkt);
}



int main(int argc, char **argv)
{
	char url[256];
	char host[128];


	int opt;
	unsigned int ret;
	unsigned int port;

	struct addrinfo *res;

	struct sigaction SIGINT_handler;

	struct sim_net_cfg sim_net;



	bzero(&sim_net,  sizeof(struct sim_net_cfg));

	/**
	 * defaults
	 */
	port     = DEFAULT_PORT;
	sprintf(url, "%s", DEFAULT_ADDR);
	sprintf(host, "%s", DEFAULT_ADDR);

	sim_net.raw      = 0; /* expect gresb format */


	while ((opt = getopt(argc, argv, "p:s:bh")) != -1) {
		switch (opt) {

		case 'p':
			port = strtol(optarg, NULL, 0);
			break;

		case 's':
			ret = getaddrinfo(optarg, NULL, NULL, &res);
			if (ret) {
				printf("error in getaddrinfo: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			sprintf(host, "%s", strtok(optarg, ":"));
			while (res) {
				if (res->ai_family == AF_INET) {
					inet_ntop(res->ai_family,
						  &((struct sockaddr_in *) res->ai_addr)->sin_addr,
						  host, sizeof(host));
					break;	/* just take the first one and hope for the best */
				}
				res = res->ai_next;
			}
			freeaddrinfo(res);
			break;


		case 'b':
			sim_net.raw = 1;
			break;

		case 'h':
		default:
			printf("\nUsage: %s [OPTIONS]\n", argv[0]);
			printf("  -p PORT                   local uplink port number (default %d)\n", port);
			printf("  -s ADDRESS                local source address (default: %s)\n", url);
			printf("  -b			    exchange raw binary data on uplink port (expect GRESB format otherwise)\n");
			printf("  -h                        print this help and exit\n");
			printf("\n");
			exit(0);
		}

	}


    	/**
	 * set up our network
	 */

	sprintf(url, "%s:%d", host, port);


	sim_net.n_fd = 0;
	sim_net.socket = bind_server_socket(url, &sim_net);

	ret = pthread_create(&sim_net.thread_accept, NULL,
			     accept_connections,
			     &sim_net);
	if (ret) {
		printf("Epic fail in pthread_create: %s\n", strerror(ret));
		exit(EXIT_FAILURE);
	}

	ret = pthread_create(&sim_net.thread_poll, NULL, poll_socket, &sim_net);
	if (ret) {
		printf("Epic fail in pthread_create: %s\n", strerror(ret));
		exit(EXIT_FAILURE);
	}

	printf("Started SIM in SERVER mode\n");


	set_tcp_nodelay(&sim_net);



	/**
	 * catch ctrl+c
	 */

	SIGINT_handler.sa_handler = sigint_handler;
	sigemptyset(&SIGINT_handler.sa_mask);
	SIGINT_handler.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_handler, NULL);



	/* initialise the FEE libraries */
	smile_fee_ctrl_init(&smile_fee_mem);

	/* main simulator */
	fee_sim_main(&sim_net);	/* should make this a thread */


	/**
	 *  wait for signal, then clean up
	 */

	printf("Ready, enter Ctrl+C to quit.\n");

	pause();

	if (sim_net.thread_accept)
		pthread_cancel(sim_net.thread_accept);

	if (sim_net.thread_accept)
		pthread_cancel(sim_net.thread_accept);

	if (sim_net.socket)
		close(sim_net.socket);

	return 0;


}
