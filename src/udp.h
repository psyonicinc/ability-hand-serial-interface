#ifndef UDP_H
#define UDP_H

#include "tinycsocket.h"
#include <cstdio>



struct UdpState {
	TcsSocket socket;
	char ip[64];
	uint16_t port;
	bool connected;
};


bool udp_connect(UdpState* state);
void udp_disconnect(UdpState* state);

#endif