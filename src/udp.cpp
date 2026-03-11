#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#endif
#define TINYCSOCKET_IMPLEMENTATION
#include "udp.h"

bool udp_connect(UdpState* state)
{
	if (state->connected)
		udp_disconnect(state);

	state->socket = TCS_SOCKET_INVALID;
	TcsResult res = tcs_socket_preset(&state->socket, TCS_PRESET_UDP_IP4);
	if (res != TCS_SUCCESS)
	{
		printf("UDP: failed to create socket (%d)\n", res);
		return false;
	}

	// Resolve IP string to TcsAddress, then connect
	struct TcsAddress remote_addr = TCS_ADDRESS_NONE;
	size_t addr_count = 0;
	res = tcs_address_resolve(state->ip, TCS_AF_IP4, &remote_addr, 1, &addr_count);
	if (res != TCS_SUCCESS || addr_count == 0)
	{
		printf("UDP: failed to resolve address '%s' (%d)\n", state->ip, res);
		tcs_close(&state->socket);
		return false;
	}
	remote_addr.data.ip4.port = state->port;

	res = tcs_connect(state->socket, &remote_addr);
	if (res != TCS_SUCCESS)
	{
		printf("UDP: failed to connect to %s:%u (%d)\n", state->ip, state->port, res);
		tcs_close(&state->socket);
		return false;
	}

	state->connected = true;
	printf("UDP: connected to %s:%u\n", state->ip, state->port);
	return true;
}


void udp_disconnect(UdpState* state)
{
	if (state->socket != TCS_SOCKET_INVALID)
	{
		tcs_close(&state->socket);
	}
	state->connected = false;
	printf("UDP: disconnected\n");
}
