/*
 * Greybus Simulator
 *
 * Copyright 2016 BayLibre.
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef _GBSIM_SOCKET_H_
#define _GBSIM_SOCKET_H_

#include <stdint.h>

void socket_init(void);
void socket_loop(void);
int socket_create(unsigned int cport_id);
void sockets_close(void);
int cport_to_socket(uint16_t cport_id);
uint16_t socket_to_cport(int socket);

#endif /* _GBSIM_SOCKET_H_ */
