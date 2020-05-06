#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <signal.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>


#include <gresb.h>


#define DEFAULT_LINK 0
#define DEFAULT_PORT 1234
#define DEFAULT_ADDR "0.0.0.0"


struct bridge_cfg {

	int socket;

	int n_fd;
	fd_set set;

	pthread_t thread_accept;
	pthread_t thread_poll;

	struct bridge_cfg *bridge;
	struct bridge_cfg *gresb_tx;
	struct bridge_cfg *gresb_rx;
};



/**
 * @brief a sigint handler, should we ever need it
 */

static void sigint_handler(__attribute__((unused)) int s)
{
	printf("\nCaught signal %d\n", s);
}


/**
 * @brief transmits the contents of a buffer from a socket to its peer
 */

static int send_all(int sock_fd, const unsigned char *buf, int len)
{
	int n;


	while (len) {

		printf("len: %d\n", len);
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
 * @brief create socket address from url
 *
 * @note url expected to be <ip>:<port>
 */

static struct sockaddr_in sockaddr_from_url(const char *url)
{
	char *str;

	struct sockaddr_in sockaddr;


	str = (char *) malloc(strlen(url));
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
 * @brief establish a client socket connection
 */

static int connect_client_socket(const char *url, struct bridge_cfg *cfg)
{
	int fd;
	int ret;

	struct sockaddr_in server;


	server = sockaddr_from_url(url);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return fd;

	ret = connect(fd, (struct sockaddr *) &server, sizeof(server));
	if (ret < 0)
		return ret;

	FD_ZERO(&cfg->set);
	FD_SET(fd, &cfg->set);

	/* update maximum file descriptor number */
	if (fd > cfg->n_fd)
		cfg->n_fd = fd;

	return fd;
}


/**
 * @brief bind a socket for a listening server
 *
 * @note url expected to be <ip>:<port>
 */

static int bind_server_socket(const char* url, struct bridge_cfg *cfg)
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

	struct bridge_cfg *cfg;

	struct sockaddr_storage client_addr;

	socklen_t client_addr_len;


	cfg = (struct bridge_cfg *) data;

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
 * @brief send user packets to the GRESB
 */

static int usr_pkt_to_gresb(int fd, struct bridge_cfg *cfg)
{
	int ret;
	ssize_t recv_bytes;
	unsigned char *recv_buffer;


	recv_buffer = malloc(GRESB_PKT_SIZE_MAX);

	recv_bytes  = recv(fd, recv_buffer, GRESB_PKT_SIZE_MAX, 0);

	for (int i = 0; i < recv_bytes; i++)
		printf("%c", recv_buffer[i]);

	/* we SEND to the TX port */
	ret = send_all(cfg->gresb_tx->socket,
		       recv_buffer, recv_bytes);
	if (ret == -1)
		perror("send");

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

	struct bridge_cfg *cfg;


	if (!data)
		return NULL;

	cfg = (struct bridge_cfg *) data;

	/* wait 10 ms in select() */
	to.tv_sec  = 0;
	to.tv_usec = 10000;

	while (1) {

		r_set = cfg->set;

		/* select ready sockets */
		if (select((cfg->n_fd + 1), &r_set, NULL, NULL, &to) <= 0) {
			usleep(1000);
			continue;
		}

		for (fd = 0; fd <= cfg->n_fd; fd++) {

			if (!FD_ISSET(fd, &r_set))
				continue;

			/* clean up disconnected socket */
			if (usr_pkt_to_gresb(fd, cfg) <= 0) {
				FD_CLR(fd, &cfg->set);
				close(fd);
			}
		}
	}

	return NULL;
}


static int gresb_pkt_to_usr(int fd, struct bridge_cfg *cfg)
{
	int ret;
	ssize_t recv_bytes;
	uint8_t *recv_buffer;




	recv_buffer = malloc(GRESB_PKT_SIZE_MAX);

	recv_bytes  = recv(fd, recv_buffer, GRESB_PKT_SIZE_MAX, 0);

	for (int i = 0; i < recv_bytes; i++)
		printf("%c", recv_buffer[i]);

	for(int fdc = 0; fdc <= cfg->bridge->n_fd; fdc++) {

		if (!FD_ISSET(fdc, &cfg->bridge->set))
			continue;

		ret = send_all(fdc,
			       gresb_get_spw_data(recv_buffer),
			       gresb_get_spw_data_size(recv_buffer));

		printf("size: %ld\n", gresb_get_spw_data_size(recv_buffer));

		if (ret == -1) {
			perror("send");
			close(fdc);
			FD_CLR(fdc, &cfg->bridge->set);
		}
	}


	free(recv_buffer);


	return recv_bytes;

}


/**
 * @brief thread function polling a GRESB network socket
 */

static void *poll_socket_gresb(void *data)
{
	int fd;

	fd_set r_set;

	struct timeval to;

	struct bridge_cfg *cfg;


	if (!data)
		return NULL;

	cfg = (struct bridge_cfg *) data;

	/* wait 10 ms in select() */
	to.tv_sec  = 0;
	to.tv_usec = 10000;

	while (1) {

		r_set = cfg->set;

		/* select ready sockets */
		if (select((cfg->n_fd + 1), &r_set, NULL, NULL, &to) <= 0) {
			usleep(1000);
			continue;
		}

		for (fd = 0; fd <= cfg->n_fd; fd++) {

			if (!FD_ISSET(fd, &r_set))
				continue;

			/* clean up disconnected socket */
			if (gresb_pkt_to_usr(fd, cfg) <= 0) {
				FD_CLR(fd, &cfg->set);
				close(fd);
			}
		}
	}

	return NULL;
}





int main(int argc, char **argv)
{
	char url[256];
	char host[128];
	char gresb[128];


	int opt;
	unsigned int ret;
	unsigned int port;
	unsigned int link;

	struct addrinfo *res;

	struct sigaction SIGINT_handler;

	enum {SERVER, CLIENT} mode;

	struct bridge_cfg bridge;
	struct bridge_cfg gresb_tx;
	struct bridge_cfg gresb_rx;



	bzero(&bridge,   sizeof(struct bridge_cfg));
	bzero(&gresb_tx, sizeof(struct bridge_cfg));
	bzero(&gresb_rx, sizeof(struct bridge_cfg));

	/**
	 * defaults
	 */
	gresb[0] = '\0';
	link     = DEFAULT_LINK;
	mode     = SERVER;
	port     = DEFAULT_PORT;
	sprintf(host, "%s", DEFAULT_ADDR);

	bridge.gresb_tx = &gresb_tx;
	bridge.gresb_rx = &gresb_rx;
	gresb_tx.bridge = &bridge;
	gresb_rx.bridge = &bridge;


	while ((opt = getopt(argc, argv, "G:L:p:s:r:h")) != -1) {
		switch (opt) {

		case 'G':
			ret = getaddrinfo(optarg, NULL, NULL, &res);
			if (ret) {
				printf("error in getaddrinfo: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			sprintf(gresb, "%s", optarg);
			while (res) {
				if (res->ai_family == AF_INET) {
					inet_ntop(res->ai_family,
						  &((struct sockaddr_in *) res->ai_addr)->sin_addr,
						  gresb, sizeof(host));
					break;	/* just take the first one and hope for the best */
				}
				res = res->ai_next;
			}
			freeaddrinfo(res);
			break;


		case 'L':
			link = strtol(optarg, NULL, 0);
			break;

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

		case 'r':
			mode = CLIENT;
			/* yes, it's totally redundant... */
			sprintf(host, "%s", strtok(optarg, ":"));
			port = strtol(strtok(NULL, ":"), NULL, 0);

			ret = getaddrinfo(optarg, NULL, NULL, &res);
			if (ret) {
				printf("error in getaddrinfo: %s\n",
				       strerror(errno));
				exit(EXIT_FAILURE);
			}
			while (res) {
				if (res->ai_family == AF_INET) {
					inet_ntop(res->ai_family,
						  &((struct sockaddr_in *) res->ai_addr)->sin_addr,
						  host, sizeof(host));
					break;
				}
				res = res->ai_next;
			}
			freeaddrinfo(res);

			break;

		case 'h':
		default:
			printf("\nUsage: %s [OPTIONS]\n", argv[0]);
			printf("  -G ADDRESS                address of the GRESP\n");
			printf("  -L LINK_ID                link id to use on GRESP\n");
			printf("  -p PORT                   local port number (default %d)\n", port);
			printf("  -s ADDRESS                local source address (default: %s\n", url);
			printf("  -r ADDRESS:PORT           client mode: address and port of remote target\n");
			printf("  -h, --help                print this help and exit\n");
			printf("\n");
			exit(0);
		}

	}



    	/**
	 * set up GRESB connection
	 */


	if (!strlen(gresb)) {
		printf("Please specify GRESB host address\n");
		exit(EXIT_FAILURE);
	}

	if (link > GRESB_VLINK_MAX) {
		printf("GRESB link must be in range 0-%d\n", GRESB_VLINK_MAX);
		exit(EXIT_FAILURE);
	}


	/* TX port on the GRESB (i.e. sent by us to NET routed to SPW) */
	sprintf(url, "%s:%d", gresb, GRESB_VLINK_TX(link));

	gresb_tx.socket = connect_client_socket(url, &gresb_tx);

	if (gresb_tx.socket < 0) {
		printf("Failed to connect to %s\n", url),
			exit(EXIT_FAILURE);
	}

	/* the GRESB answers to configuration  requests on the TX port */
	ret = pthread_create(&gresb_tx.thread_poll, NULL,
			     poll_socket_gresb, &gresb_tx);
	if (ret) {
		printf("Epic fail in pthread_create: %s\n", strerror(ret));
		exit(EXIT_FAILURE);
	}

	printf("Connected to GRESB TX\n");



	/* RX port on the GRESB (i.e. route from SPW to NET, received by us */
	sprintf(url, "%s:%d", gresb, GRESB_VLINK_RX(link));

	gresb_rx.socket = connect_client_socket(url, &gresb_rx);

	if (gresb_rx.socket < 0) {
		printf("Failed to connect to %s\n", url),
			exit(EXIT_FAILURE);
	}

	ret = pthread_create(&gresb_rx.thread_poll, NULL,
			     poll_socket_gresb, &gresb_rx);
	if (ret) {
		printf("Epic fail in pthread_create: %s\n", strerror(ret));
		exit(EXIT_FAILURE);
	}

	printf("Connected to GRESB RX\n");



    	/**
	 * set up our network
	 */

	sprintf(url, "%s:%d", host, port);

	if (mode == SERVER) {

		bridge.n_fd = 0;
		bridge.socket = bind_server_socket(url, &bridge);

		ret = pthread_create(&bridge.thread_accept, NULL,
				     accept_connections,
				     &bridge);
		if (ret) {
			printf("Epic fail in pthread_create: %s\n", strerror(ret));
			exit(EXIT_FAILURE);
		}

		ret = pthread_create(&bridge.thread_poll, NULL, poll_socket, &bridge);
		if (ret) {
			printf("Epic fail in pthread_create: %s\n", strerror(ret));
			exit(EXIT_FAILURE);
		}

		printf("Started in SERVER mode\n");

	} else {

		bridge.socket = connect_client_socket(url, &bridge);

		if (bridge.socket < 0) {
			printf("Failed to connect to %s\n", url),
				exit(EXIT_FAILURE);
		}

		ret = pthread_create(&bridge.thread_poll, NULL, poll_socket, &bridge);
		if (ret) {
			printf("Epic fail in pthread_create: %s\n", strerror(ret));
			exit(EXIT_FAILURE);
		}

		printf("Started in CLIENT mode\n");
	}


	/**
	 * catch ctrl+c
	 */

	SIGINT_handler.sa_handler = sigint_handler;
	sigemptyset(&SIGINT_handler.sa_mask);
	SIGINT_handler.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_handler, NULL);



	/**
	 *  wait for signal, then clean up
	 */

	printf("Ready...\n");

	pause();

	if (bridge.thread_accept)
		pthread_cancel(bridge.thread_accept);

	if (bridge.thread_accept)
		pthread_cancel(bridge.thread_accept);

	if (gresb_tx.thread_poll)
		pthread_cancel(gresb_tx.thread_poll);

	if (gresb_rx.thread_poll)
		pthread_cancel(gresb_rx.thread_poll);

	if (bridge.socket)
		close(bridge.socket);

	if (gresb_rx.socket)
		close(gresb_rx.socket);

	if (gresb_tx.socket)
		close(gresb_tx.socket);
}
