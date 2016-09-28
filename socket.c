/*
 * Greybus Simulator
 *
 * Copyright 2016 BayLibre.
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "gbsim.h"

#define GB_PORT	8880
#define GB_IP "127.0.0.1"

struct cport
{
	TAILQ_ENTRY(cport) cnode;
	uint16_t id;
	int socket;
	int serv_socket;
	pthread_t thread;
};
TAILQ_HEAD(cport_chead, cport) cports;

pthread_t sock_thread;

void *listen_thread(void *data);

struct cport *lookup_cport(uint16_t cport_id)
{
	struct cport *cport;

	TAILQ_FOREACH(cport, &cports, cnode) {
		if (cport->id == cport_id)
			return cport;
	}

	return NULL;
}

void cport_create(uint16_t cport_id, int sockfd)
{
	int ret;
	struct cport *cport;

	cport = lookup_cport(cport_id);
	if (cport)
		return;

	cport = malloc(sizeof(*cport));
	cport->id = cport_id;
	cport->serv_socket = sockfd;
	TAILQ_INSERT_TAIL(&cports, cport, cnode);

	ret = pthread_create(&cport->thread, NULL,
			     listen_thread, (void *)cport);
	if (ret < 0) {
		socket_close(sockfd);
	}
}

int cport_to_socket(uint16_t cport_id)
{
	struct cport *cport;

	cport = lookup_cport(cport_id);
	if (!cport) {
		return -EINVAL;
	}

	return cport->socket;
}

uint16_t socket_to_cport(int socket)
{
	struct cport *cport;

	TAILQ_FOREACH(cport, &cports, cnode) {
		if (cport->socket == socket)
			return cport->id;
	}

	return -1;
}

void socket_loop(void)
{
	pthread_join(sock_thread, NULL);
}

void sockets_close(void)
{

}

void socket_close(int sockfd)
{
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);

	pthread_cancel(sock_thread);
}

void *listen_thread(void *data)
{
	struct sockaddr_in ap_addr;
	socklen_t ap_addr_len;
	struct cport *cport = data;

	printf("start listening on fd %x\n", cport->serv_socket);
	if (listen(cport->serv_socket, 1)) {
		perror("Failed to listen");
		return NULL;
	}

	/* Accept actual connection from the client */
	cport->socket = accept(cport->serv_socket,
			       (struct sockaddr *)&ap_addr, &ap_addr_len);

	if (cport->socket < 0) {
		return NULL;
	}

	return recv_thread((void *)cport->socket);
}

void socket_init(void)
{
	TAILQ_INIT(&cports);
}

int socket_create(unsigned int cport_id)
{
	int ret;
	int sockfd;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("Can't create socket\n");
		return sockfd;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(GB_PORT + cport_id);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (ret < 0) {
		printf("Failed on binding");
		return ret;
	}

	cport_create(cport_id, sockfd);

	return 0;
}
