#define TINYCSOCKET_IMPLEMENTATION
#include "udp.h"

bool udp_connect(UdpState* state)
{
	if (state->connected)
		udp_disconnect(state);

	tcs_lib_init();

	state->socket = TCS_NULLSOCKET;
	TcsReturnCode res = tcs_create(&state->socket, TCS_TYPE_UDP_IP4);
	if (res != TCS_SUCCESS)
	{
		printf("UDP: failed to create socket (%d)\n", res);
		tcs_lib_free();
		return false;
	}

	// Resolve IP string to TcsAddress, then connect
	struct TcsAddress remote_addr = TCS_ADDRESS_NULL;
	size_t addr_count = 0;
	res = tcs_resolve_hostname(state->ip, TCS_AF_IP4, &remote_addr, 1, &addr_count);
	if (res != TCS_SUCCESS || addr_count == 0)
	{
		printf("UDP: failed to resolve address '%s' (%d)\n", state->ip, res);
		tcs_destroy(&state->socket);
		tcs_lib_free();
		return false;
	}
	remote_addr.data.af_inet.port = state->port;

	res = tcs_connect_address(state->socket, &remote_addr);
	if (res != TCS_SUCCESS)
	{
		printf("UDP: failed to connect to %s:%u (%d)\n", state->ip, state->port, res);
		tcs_destroy(&state->socket);
		tcs_lib_free();
		return false;
	}

	state->connected = true;
	printf("UDP: connected to %s:%u\n", state->ip, state->port);
	return true;
}


void udp_disconnect(UdpState* state)
{
	if (state->socket != TCS_NULLSOCKET)
	{
		tcs_destroy(&state->socket);
		tcs_lib_free();
	}
	state->connected = false;
	printf("UDP: disconnected\n");
}
