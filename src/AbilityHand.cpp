#include "AbilityHand.h"
#include <cstring>
#include <cmath>
#include <chrono>
#include "abh_crc.h"

AbilityHand::AbilityHand()
{
	// Initialize hand state
	memset(&hand_state, 0, sizeof(hand_state));
	hand_state.address = 0x50;  // Default address
	hand_state.command_header = FIXED_POSITION_CONTROL_TX1;  // Default mode

	// Initialize statistics
	memset(&stats_, 0, sizeof(stats_));

	// Zero all buffer memory
	memset(tx_mem_, 0, sizeof(tx_mem_));
	memset(tx_stuffed_mem_, 0, sizeof(tx_stuffed_mem_));
	memset(rx_stuffed_mem_, 0, sizeof(rx_stuffed_mem_));
	memset(rx_unstuff_mem_, 0, sizeof(rx_unstuff_mem_));
	iface_ = IFACE_SERIAL;
	stuffing_mode = STUFFING_PPP;
	m_socket_.socket = TCS_NULLSOCKET;
	snprintf(m_socket_.ip, sizeof(m_socket_.ip), "192.168.1.100");
	m_socket_.port = 5000;
	m_socket_.connected = false;
	start_time();



	// Initialize TX buffers
	tx_frame_.buf = tx_mem_;
	tx_frame_.size = sizeof(tx_mem_);
	tx_frame_.len = 0;

	tx_unstuff_alias_.buf = tx_mem_;
	tx_unstuff_alias_.size = sizeof(tx_mem_);
	tx_unstuff_alias_.length = 0;

	tx_stuff_buf_.buf = tx_stuffed_mem_;
	tx_stuff_buf_.size = sizeof(tx_stuffed_mem_);
	tx_stuff_buf_.length = 0;

	tx_cobs_buf_.buf = tx_stuffed_mem_;
	tx_cobs_buf_.size = sizeof(tx_stuffed_mem_);
	tx_cobs_buf_.length = 0;
	tx_cobs_buf_.encoded_state = COBS_DECODED;

	// Initialize RX buffers
	rx_buffer_alias_.buf = rx_stuffed_mem_;
	rx_buffer_alias_.size = sizeof(rx_stuffed_mem_);
	rx_buffer_alias_.length = 0;

	rx_cobs_encoded_.buf = rx_stuffed_mem_;
	rx_cobs_encoded_.size = sizeof(rx_stuffed_mem_);
	rx_cobs_encoded_.length = 0;
	rx_cobs_encoded_.encoded_state = COBS_ENCODED;

	rx_ppp_unstuff_alias_.buf = rx_unstuff_mem_;
	rx_ppp_unstuff_alias_.size = sizeof(rx_unstuff_mem_);
	rx_ppp_unstuff_alias_.length = 0;

	rx_cobs_decoded_.buf = rx_unstuff_mem_;
	rx_cobs_decoded_.size = sizeof(rx_unstuff_mem_);
	rx_cobs_decoded_.length = 0;
	rx_cobs_decoded_.encoded_state = COBS_DECODED;

	//initalize decode buffer. Destination/unstuff buffer
	rx_frame.buf = rx_unstuff_mem_;
	rx_frame.size = sizeof(rx_unstuff_mem_);
	rx_frame.len = 0;
}

AbilityHand::~AbilityHand()
{
	disconnect();
	if (iface_ == IFACE_UDP)
	{
		udp_disconnect(&m_socket_);
	}
}


static int unstuff_stream(abh_buffer_t * dest, bytestream_t * stream, AbilityHand::StuffingMode mode)
{
	if(dest == NULL || stream == NULL)
	{
		return -1;
	}

	if(mode == AbilityHand::STUFFING_PPP)
	{
		ppp_buffer_t ppp_stuffed = {stream->buf, stream->size, stream->len};
		ppp_buffer_t ppp_unstuffed = {dest->buf, dest->size, dest->len};
		size_t rc = PPP_unstuff(&ppp_unstuffed, &ppp_stuffed);
		dest->len = ppp_unstuffed.length;
		if(rc != 0)
		{
			return ABH_SUCCESS;
		}
		
	}
	else if(mode == AbilityHand::STUFFING_COBS)
	{
		cobs_buf_t	cobs_encoded = {stream->buf, stream->size, stream->len, COBS_ENCODED};
		cobs_buf_t cobs_decoded = {dest->buf, dest->size, dest->len, COBS_DECODED};
		int rc = cobs_decode_double_buffer(&cobs_encoded, &cobs_decoded);
		dest->len = cobs_decoded.length;
		if(rc == COBS_SUCCESS)
		{
			return ABH_SUCCESS;
		}
	}
	return ABH_ERROR_INVALID_ARGUMENT;
}

static bool _motion_data_callback(bytestream_t * stream, void * context)
{
	if(context == NULL)
	{
		return false;	//guard. This forces read_until_valid to poll until timeout
	}
	AbilityHand * hand = (AbilityHand*)context;

	int rc = unstuff_stream(&hand->rx_frame, stream, hand->stuffing_mode);
	if(rc != ABH_SUCCESS)	//must match expected unstuffing mode or bounce as invalid
	{
		return false;
	}
	rc = abh_parse_movement_reply(&hand->rx_frame, &hand->hand_state);
	if(rc == ABH_SUCCESS)
	{
		return true;
	}

	return false;
}

/*Should consist of ONLY pointers*/
typedef struct RegisterContext
{
	AbilityHand * hand;
	abh_register_reply_t * reg_reply;
}RegisterContext;

/*
Contract:
unstuff the stream and pass it to abh_parse_register_reply
On success, return true.
On fail, return false.
Easy!
*/
static bool _abh_register_callback(bytestream_t * stream, void * context)
{
	if(context == NULL)
	{
		return false;	//guard. This forces read_until_valid to poll until timeout
	}

	RegisterContext * ctx = (RegisterContext*)context;
	AbilityHand * hand = ctx->hand;
	abh_register_reply_t * reply = ctx->reg_reply;

	int rc = unstuff_stream(&hand->rx_frame, stream, hand->stuffing_mode);
	if(rc != ABH_SUCCESS)	//must match expected unstuffing mode or bounce as invalid
	{
		return false;
	}

	reply->register_value = 0;
	reply->reply_type = ABH_REGISTER_INVALID_REPLY;
	reply->request_valid = false;
	rc = abh_parse_register_reply(&hand->rx_frame, reply);
	if(rc == ABH_SUCCESS)
	{
		return true;
	}
	return false;
}


typedef struct CommandHandlingContext
{
	AbilityHand * hand;
	std::vector<std::string> * replies;
	int num_expected_replies;	//must initialize!
	int reply_count;	//must initialize to zero!
}CommandHandlingContext;

static bool _command_handling_callback(bytestream_t * stream, void * context)
{
	if(context == NULL)
	{
		return false;	//guard. This forces read_until_valid to poll until timeout
	}
	const size_t header_len = 2;
	const size_t crc_len = 2;

	CommandHandlingContext * ctx = (CommandHandlingContext*)context;
	AbilityHand * hand = ctx->hand;
	std::vector<std::string> * replies = ctx->replies;

	int rc = unstuff_stream(&hand->rx_frame, stream, hand->stuffing_mode);
	if(rc != ABH_SUCCESS)	//must match expected unstuffing mode or bounce as invalid
	{
		return false;
	}

	if (hand->rx_frame.len < crc_len + 1)
	{
		return false;
	}

	/*TODO: fix this with proper silent retry on invalid frame reception, matching other calls*/
	size_t rx_payload_len = hand->rx_frame.len - crc_len;
	uint16_t expected_crc = abh_crc16(hand->rx_frame.buf, rx_payload_len);
	uint16_t received_crc = (uint16_t)hand->rx_frame.buf[rx_payload_len] |
							((uint16_t)hand->rx_frame.buf[rx_payload_len + 1] << 8);
	if (expected_crc != received_crc)
		return false;

	replies->emplace_back(reinterpret_cast<char*>(hand->rx_frame.buf), rx_payload_len);
	ctx->reply_count++;

	if(ctx->reply_count == ctx->num_expected_replies)	//single necessary return condition
	{
		return true;
	}
	return false;
}

std::vector<UdpEndpoint> AbilityHand::discover_forwarding_servers(uint32_t timeout)
{
	std::vector<UdpEndpoint> results;

	tcs_lib_init();

	TcsSocket sock = TCS_NULLSOCKET;
	TcsReturnCode res = tcs_create(&sock, TCS_TYPE_UDP_IP4);
	if (res != TCS_SUCCESS)
	{
		printf("Discovery: failed to create socket (%d)\n", res);
		tcs_lib_free();
		return results;
	}

	tcs_set_broadcast(sock, true);

	struct TcsAddress local_addr = TCS_ADDRESS_NULL;
	local_addr.family = TCS_AF_IP4;
	local_addr.data.af_inet.address = 0;  // INADDR_ANY — replies come back to this socket
	local_addr.data.af_inet.port = 0;
	tcs_bind_address(sock, &local_addr);

	// Send WHO_GOES_THERE on every non-loopback IPv4 interface's subnet broadcast
	struct TcsInterface ifaces[16];
	size_t iface_count = 0;
	if (tcs_local_interfaces(ifaces, 16, &iface_count) != TCS_SUCCESS)
		printf("Discovery: failed to enumerate interfaces\n");

	int sent_count = 0;
	for (size_t i = 0; i < iface_count; i++)
	{
		if (ifaces[i].address.family != TCS_AF_IP4)
			continue;
		uint32_t addr = ifaces[i].address.data.af_inet.address;
		if ((addr >> 24) == 127)  // skip loopback
			continue;

		uint32_t bcast_ip = (addr & 0xFFFFFF00) | 0xFF;
		printf("Discovery: trying '%s' %u.%u.%u.%u -> broadcast %u.%u.%u.%u\n",
			ifaces[i].name,
			(addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF,
			(bcast_ip >> 24) & 0xFF, (bcast_ip >> 16) & 0xFF, (bcast_ip >> 8) & 0xFF, bcast_ip & 0xFF);

		struct TcsAddress bcast_addr = TCS_ADDRESS_NULL;
		bcast_addr.family = TCS_AF_IP4;
		bcast_addr.data.af_inet.address = bcast_ip;
		bcast_addr.data.af_inet.port = 5067;

		size_t sent = 0;
		res = tcs_send_to(sock, (const uint8_t*)"WHO_GOES_THERE", 14, TCS_NO_FLAGS, &bcast_addr, &sent);
		if (res != TCS_SUCCESS)
		{
			printf("Discovery: send failed on '%s' (%d), skipping\n", ifaces[i].name, res);
			continue;
		}
		printf("Discovery: sent %zu bytes on '%s'\n", sent, ifaces[i].name);
		sent_count++;
	}

	if (sent_count == 0)
	{
		printf("Discovery: no interfaces succeeded\n");
		tcs_destroy(&sock);
		tcs_lib_free();
		return results;
	}

	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	uint8_t buf[256];

	while (true)
	{
		int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - start).count();
		if (elapsed >= (int64_t)timeout)
		{
			break;
		}

		uint32_t remaining = (uint32_t)(timeout - (uint32_t)elapsed);
		uint32_t recv_timeout = remaining < 50u ? remaining : 50u;
		tcs_set_receive_timeout(sock, (int)recv_timeout);

		struct TcsAddress src_addr = TCS_ADDRESS_NULL;
		size_t received = 0;
		res = tcs_receive_from(sock, buf, sizeof(buf), TCS_NO_FLAGS, &src_addr, &received);
		if (res == TCS_SUCCESS && received > 0)
		{
			char ip_str[64];
			uint32_t raw = src_addr.data.af_inet.address;
			snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u",
				(raw >> 24) & 0xFF, (raw >> 16) & 0xFF,
				(raw >> 8) & 0xFF, raw & 0xFF);

			bool already_found = false;
			for (const UdpEndpoint& ep : results)
			{
				if (strncmp(ep.ip, ip_str, sizeof(ep.ip)) == 0)
				{
					already_found = true;
					break;
				}
			}
			if (!already_found)
			{
				printf("Discovery: found server at %s:5067\n", ip_str);
				UdpEndpoint ep;
				snprintf(ep.ip, sizeof(ep.ip), "%s", ip_str);
				ep.port = 5067;
				results.push_back(ep);
			}
		}
	}

	tcs_destroy(&sock);
	tcs_lib_free();
	return results;
}

int AbilityHand::init_udp(const UdpEndpoint& endpoint)
{
	return init_udp(endpoint.ip, endpoint.port);
}

int AbilityHand::init_udp(const char * ip, uint16_t port)
{
	for(int i = 0; i < sizeof(m_socket_.ip); i++)
	{
		m_socket_.ip[i] = 0;
	}
	int retl = snprintf(m_socket_.ip, sizeof(m_socket_.ip), "%s", ip);		//consider checking returned size to avoid issues related to null termination
	if(retl >= sizeof(m_socket_.ip))
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	bool is_null_terminated = false;
	for(int i = 0; i < sizeof(m_socket_.ip); i++)
	{
		if(m_socket_.ip[i] == 0)
		{
			is_null_terminated = true;
			break;
		}
	}
	if(is_null_terminated == false)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}

	m_socket_.port = port;
	bool success = udp_connect(&m_socket_);
	if(success)
	{
		iface_ = IFACE_UDP;
		return ABH_SUCCESS;
	}
	else
	{
		printf("Failed to connect to socket\n");
		return ABH_ERROR_NOT_CONNECTED;
	}
}

int AbilityHand::init(unsigned long baud_rate)
{
	if (serial_.connected())
	{
		return ABH_ERROR_ALREADY_CONNECTED;
	}

	if (!serial_.autoconnect(baud_rate))
	{
		return ABH_ERROR_CONNECTION_FAILED;
	}


	reset_statistics();

	return ABH_SUCCESS;
}

int AbilityHand::init(const char* port_name, unsigned long baud_rate)
{
	if (serial_.connected())
	{
		return ABH_ERROR_ALREADY_CONNECTED;
	}

	if (!serial_.connect(port_name, baud_rate))
	{
		return ABH_ERROR_CONNECTION_FAILED;
	}

	
	reset_statistics();

	return ABH_SUCCESS;
}

void AbilityHand::disconnect()
{
	serial_.disconnect();
}

bool AbilityHand::is_connected() const
{
	if(iface_ == IFACE_SERIAL)
	{
		// Cast away const since Serial::connected() is not const
		return const_cast<Serial&>(serial_).connected();
	}
	else
	{
		return m_socket_.connected;
	}
}

void AbilityHand::start_time()
{
	timestamp_ = std::chrono::steady_clock::now();
}

int64_t AbilityHand::get_ms() const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - timestamp_).count();
}


int AbilityHand::set_motion_mode(MotionMode mode)
{
	if (!is_connected())
	{
		return ABH_ERROR_NOT_CONNECTED;
	}

	// Map enum to command header
	switch (mode)
	{
		case POSITION_MODE_TX1:
			hand_state.command_header = FIXED_POSITION_CONTROL_TX1;
			break;
		case POSITION_MODE_TX2:
			hand_state.command_header = FIXED_POSITION_CONTROL_TX2;
			break;
		case POSITION_MODE_TX3:
			hand_state.command_header = FIXED_POSITION_CONTROL_TX3;
			break;
		case VELOCITY_MODE_TX1:
			hand_state.command_header = FIXED_VELOCITY_CONTROL_TX1;
			break;
		case VELOCITY_MODE_TX2:
			hand_state.command_header = FIXED_VELOCITY_CONTROL_TX2;
			break;
		case VELOCITY_MODE_TX3:
			hand_state.command_header = FIXED_VELOCITY_CONTROL_TX3;
			break;
		case TORQUE_MODE_TX1:
			hand_state.command_header = FIXED_TORQUE_CONTROL_TX1;
			break;
		case TORQUE_MODE_TX2:
			hand_state.command_header = FIXED_TORQUE_CONTROL_TX2;
			break;
		case TORQUE_MODE_TX3:
			hand_state.command_header = FIXED_TORQUE_CONTROL_TX3;
			break;
		case VOLTAGE_MODE_TX1:
			hand_state.command_header = FIXED_VOLTAGE_CONTROL_TX1;
			break;
		case VOLTAGE_MODE_TX2:
			hand_state.command_header = FIXED_VOLTAGE_CONTROL_TX2;
			break;
		case VOLTAGE_MODE_TX3:
			hand_state.command_header = FIXED_VOLTAGE_CONTROL_TX3;
			break;
		default:
			return ABH_ERROR_INVALID_HEADER;
	}

	return ABH_SUCCESS;
}

int AbilityHand::set_short_mode(ShortMode mode)
{
	if (!is_connected())
	{
		return ABH_ERROR_NOT_CONNECTED;
	}

	// Map enum to command header
	switch (mode)
	{
		case DUMMY_READ_TX1:
			hand_state.command_header = FIXED_DUMMY_TX1;
			break;
		case DUMMY_READ_TX2:
			hand_state.command_header = FIXED_DUMMY_TX2;
			break;
		case DUMMY_READ_TX3:
			hand_state.command_header = FIXED_DUMMY_TX3;
			break;
		default:
			return ABH_ERROR_INVALID_HEADER;
	}

	return ABH_SUCCESS;
}

int AbilityHand::enable_upsampling(UpsamplingChannel mask)
{
	abh_register_reply_t reply = {};
	int rc = read_register(63, reply);
	if (rc != ABH_SUCCESS || reply.request_valid == false)
	{ 
		return rc;
	}

	uint32_t val = reply.register_value | mask;
	return write_register(63, val, reply);
}

int AbilityHand::disable_upsampling(UpsamplingChannel mask)
{
	abh_register_reply_t reply = {};
	int rc = read_register(63, reply);
	if (rc != ABH_SUCCESS || reply.request_valid == false)
	{
		return rc;
	}

	uint32_t val = reply.register_value & ~mask;
	return write_register(63, val, reply);
}

int AbilityHand::write_register(uint32_t addr, uint32_t value, abh_register_reply_t& reply, int timeout_ms)
{
	if (!is_connected())
		return ABH_ERROR_NOT_CONNECTED;

	tx_frame_.len = 0;
	int rc = abh_create_write_register_frame(&hand_state, addr, value, &tx_frame_);
	if (rc != ABH_SUCCESS)
		return rc;

	/*create a context for registration of read callback*/
	RegisterContext reg_context = 
	{
		this,
		&reply
	};
	register_callback(&_abh_register_callback, &reg_context);

	rc = stuff_and_write();
	if (rc != ABH_SUCCESS) 
	{
		return rc;
	}

	rc = read_and_parse(timeout_ms);
	if (rc != ABH_SUCCESS)
	{
		stats_.num_parse_errors++;
	}
	else
	{
		stats_.num_successful_reads++;
	}
	return rc;
}

int AbilityHand::read_register(uint32_t addr, abh_register_reply_t& reply, int timeout_ms)
{
	if (!is_connected())
		return ABH_ERROR_NOT_CONNECTED;

	tx_frame_.len = 0;
	int rc = abh_create_read_register_frame(&hand_state, addr, &tx_frame_);
	if (rc != ABH_SUCCESS)
		return rc;

	/*create a context for registration of read callback*/
	RegisterContext reg_context = 
	{
		this,
		&reply
	};
	register_callback(&_abh_register_callback, &reg_context);

	rc = stuff_and_write();
	if (rc != ABH_SUCCESS) 
	{
		return rc;
	}

	rc = read_and_parse(timeout_ms);
	if (rc != ABH_SUCCESS)
	{
		stats_.num_parse_errors++;
	}
	else
	{
		stats_.num_successful_reads++;
	}
	return rc;
}

int AbilityHand::exchange_motion_data(int timeout_ms)
{
	if (!is_connected())
		return ABH_ERROR_NOT_CONNECTED;

	int rc = abh_create_movement_frame(&hand_state, &tx_frame_);
	if (rc != ABH_SUCCESS)
		return rc;

	register_callback(&_motion_data_callback, this);

	rc = stuff_and_write();
	if (rc != ABH_SUCCESS)
	{
		return rc;
	}

	rc = read_and_parse(timeout_ms);
	if (rc != ABH_SUCCESS)
	{
		stats_.num_parse_errors++;
	}
	else
	{
		stats_.num_successful_reads++;
	}
	return rc;
}

int AbilityHand::exchange_short_msg(int timeout_ms)
{
	if (!is_connected())
		return ABH_ERROR_NOT_CONNECTED;

	int rc = abh_create_short_api_frame(&hand_state, &tx_frame_);
	if (rc != ABH_SUCCESS)
		return rc;

	register_callback(&_motion_data_callback, this);

	rc = stuff_and_write();
	if (rc != ABH_SUCCESS)
	{
		return rc;
	}

	rc = read_and_parse(timeout_ms);
	if (rc != ABH_SUCCESS)
	{
		stats_.num_parse_errors++;
	}
	else
	{
		stats_.num_successful_reads++;
	}
	return rc;
}

int AbilityHand::stuff_and_write_serial()
{
	int bytes_written = 0;
	if (stuffing_mode == STUFFING_PPP)
	{
		abh_to_ppp(&tx_frame_, &tx_unstuff_alias_);

		tx_stuff_buf_.buf[0] = FRAME_CHAR;
		ppp_buffer_t ppp_tx_alias = {&tx_stuff_buf_.buf[1], tx_stuff_buf_.size-1, 0};
		PPP_stuff(&tx_unstuff_alias_, &ppp_tx_alias);
		tx_stuff_buf_.length = ppp_tx_alias.length + 1;

		for(int i = 0; i < 10; i++)
		{
			tx_stuff_buf_.buf[tx_stuff_buf_.length++] = 0;
		}

		bytes_written = serial_.write(tx_stuff_buf_.buf, (int)tx_stuff_buf_.length);
	}
	else
	{
		abh_to_cobs(&tx_frame_, &tx_cobs_buf_);
		if (cobs_encode_single_buffer(&tx_cobs_buf_) != COBS_SUCCESS)
			return ABH_ERROR_WRITE_FAILED;
		bytes_written = serial_.write(tx_cobs_buf_.buf, (int)tx_cobs_buf_.length);
	}
	stats_.num_writes++;
	if (bytes_written <= 0)
		return ABH_ERROR_WRITE_FAILED;
	return ABH_SUCCESS;
}

int AbilityHand::read_and_parse_serial(int timeout_ms)
{
	if (stuffing_mode == STUFFING_PPP)
	{
		int rxlen = serial_.read_until_valid(rx_buffer_alias_.buf, (int)rx_buffer_alias_.size, '~', timeout_ms);
		if (rxlen == Serial::ERROR_READ_TIMEOUT)
		{
			stats_.num_timeouts++;
			return ABH_ERROR_TIMEOUT;
		}
		if (rxlen <= 0)
		{
			return ABH_ERROR_INVALID_ARGUMENT;
		}
	}
	else
	{
		int rxlen = serial_.read_until_valid(rx_cobs_encoded_.buf, (int)rx_cobs_encoded_.size, '\0', timeout_ms);
		if (rxlen == -2)
		{
			stats_.num_timeouts++;
			return ABH_ERROR_TIMEOUT;
		}
		if (rxlen <= 0)
		{
			return ABH_ERROR_INVALID_ARGUMENT;
		}
	}
	return ABH_SUCCESS;
}

int AbilityHand::stuff_and_write_udp()
{
	if (!m_socket_.connected)
		return ABH_ERROR_NOT_CONNECTED;

	size_t bytes_sent = 0;
	TcsReturnCode res;
	if (stuffing_mode == STUFFING_PPP)
	{
		abh_to_ppp(&tx_frame_, &tx_unstuff_alias_);

		tx_stuff_buf_.buf[0] = FRAME_CHAR;
		ppp_buffer_t ppp_tx_alias = {&tx_stuff_buf_.buf[1], tx_stuff_buf_.size-1, 0};
		PPP_stuff(&tx_unstuff_alias_, &ppp_tx_alias);
		tx_stuff_buf_.length = ppp_tx_alias.length + 1;


		res = tcs_send(m_socket_.socket, tx_stuff_buf_.buf, tx_stuff_buf_.length, TCS_NO_FLAGS, &bytes_sent);
	}
	else
	{
		abh_to_cobs(&tx_frame_, &tx_cobs_buf_);
		if (cobs_encode_single_buffer(&tx_cobs_buf_) != COBS_SUCCESS)
			return ABH_ERROR_WRITE_FAILED;
		res = tcs_send(m_socket_.socket, tx_cobs_buf_.buf, tx_cobs_buf_.length, TCS_NO_FLAGS, &bytes_sent);
	}
	if (res == TCS_SUCCESS)
		stats_.num_writes++;
	else
		return ABH_ERROR_WRITE_FAILED;
	return ABH_SUCCESS;
}

/*
Wrapper function to track callback registration within ability hand class scope
*/
int AbilityHand::register_callback(Serial::frame_cb_t cb, void * user_context)
{
	if(cb == NULL)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	_abh_frame_parser_cb = cb;
	_abh_parser_context = user_context;
	return serial_.register_callback(_abh_frame_parser_cb, user_context);
}

int AbilityHand::read_and_parse_udp(int timeout_ms)
{
	if (!m_socket_.connected)
		return ABH_ERROR_NOT_CONNECTED;

	struct TcsAddress src;
	size_t bytes_received = 0;
	TcsReturnCode res;
	tcs_set_receive_timeout(m_socket_.socket, timeout_ms);

	bytestream_t stream = {};	

	if (stuffing_mode == STUFFING_PPP)
	{
		res = tcs_receive_from(m_socket_.socket, rx_buffer_alias_.buf, rx_buffer_alias_.size, TCS_NO_FLAGS, &src, &bytes_received);
		if (res != TCS_SUCCESS || bytes_received == 0)
			return ABH_ERROR_TIMEOUT;
		rx_buffer_alias_.length = bytes_received;

		stream.buf = rx_buffer_alias_.buf;
		stream.size = rx_buffer_alias_.size;
		stream.len = rx_buffer_alias_.length;
	}
	else
	{
		res = tcs_receive_from(m_socket_.socket, rx_cobs_encoded_.buf, rx_cobs_encoded_.size, TCS_NO_FLAGS, &src, &bytes_received);
		if (res != TCS_SUCCESS || bytes_received == 0)
			return ABH_ERROR_TIMEOUT;
		rx_cobs_encoded_.length = bytes_received;
		
		stream.buf = rx_cobs_encoded_.buf;
		stream.size = rx_cobs_encoded_.size;
		stream.len = rx_cobs_encoded_.length;
	}
	bool valid = _abh_frame_parser_cb(&stream, _abh_parser_context);		
	if(!valid)
	{
		return ABH_ERROR_INVALID_ARGUMENT;	//general error. 
	}
	return ABH_SUCCESS;
}



int AbilityHand::stuff_and_write()
{
	if (iface_ == IFACE_SERIAL)
	{
		return stuff_and_write_serial();
	}
	else
	{
		return stuff_and_write_udp();
	}
}

int AbilityHand::read_and_parse(int timeout_ms)
{
	if (iface_ == IFACE_SERIAL)
	{
		return read_and_parse_serial(timeout_ms);
	}
	else
	{
		return read_and_parse_udp(timeout_ms);
	}
}


bool pop_crc16(abh_buffer_t * msg)
{
	if(abh_check_msg(msg) != ABH_SUCCESS)
	{
		return false;
	}
	if(msg->len <= sizeof(uint16_t))//reply must be at least 3 bytes for a 2-byte crc signature to be valid
	{
		return false;
	}
	uint16_t crc = abh_crc16(msg->buf, msg->len - sizeof(uint16_t));
    uint16_t m_crc = 0;
    unsigned char * pchecksum = msg->buf + (msg->len - sizeof(uint16_t));
    m_crc |= (uint16_t)pchecksum[0];
    m_crc |= ((uint16_t)pchecksum[1]) << 8;

	if(m_crc == crc)
	{
		msg->len = msg->len - sizeof(uint16_t);	//remove checksum
		return true;
	}
	else
	{
		return false;
	}
}

int AbilityHand::command_handling(const std::string& payload,
								   std::vector<std::string>& responses,
								   int nresponses_expected, int timeout_ms)
{
	// Build TX frame: [address][0x77][payload...][crc16_lo][crc16_hi]
	const size_t header_len = 2;
	const size_t crc_len = 2;
	size_t frame_len = header_len + payload.size() + crc_len;
	if (frame_len > tx_frame_.size)
	{
		return ABH_ERROR_BUFFER_OVERRUN;
	}

	tx_frame_.buf[0] = hand_state.address;
	tx_frame_.buf[1] = 0x77;
	memcpy(tx_frame_.buf + header_len, payload.data(), payload.size());

	uint16_t crc = abh_crc16(tx_frame_.buf, header_len + payload.size());
	tx_frame_.buf[header_len + payload.size()]	 = (uint8_t)(crc & 0xFF);
	tx_frame_.buf[header_len + payload.size() + 1] = (uint8_t)((crc >> 8) & 0xFF);
	tx_frame_.len = frame_len;

	CommandHandlingContext ctx = {
		this,
		&responses,
		nresponses_expected,
		0
	};
	register_callback(&_command_handling_callback, &ctx);
	responses.clear();

	int rc = stuff_and_write();
	if (rc != ABH_SUCCESS)
	{
		return rc;
	}

	rc = read_and_parse(timeout_ms);
	if (rc != ABH_SUCCESS)
	{
		stats_.num_parse_errors++;
	}
	else
	{
		stats_.num_successful_reads++;
	}
	return rc;
}


void AbilityHand::abh_to_ppp(abh_buffer_t* in, ppp_buffer_t* out)
{
	out->buf = in->buf;
	out->length = in->len;
	out->size = in->size;
}

void AbilityHand::abh_to_cobs(abh_buffer_t* in, cobs_buf_t* out)
{
	memcpy(out->buf, in->buf, in->len);	//must copy first because cobs encoding copies in place, as opposed to PPP which inherently performs a copy
	out->length = in->len;
	out->encoded_state = COBS_DECODED;
}

void AbilityHand::set_position_targets(const float positions_deg[NUM_CHANNELS])
{
	for (int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		hand_state.q_desired[ch] = (int16_t)(positions_deg[ch] * POSITION_SCALE);
	}
}

void AbilityHand::set_velocity_targets(const float velocities_deg_per_sec[NUM_CHANNELS])
{
	for (int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		hand_state.velocity_desired[ch] = (int16_t)(velocities_deg_per_sec[ch] * VELOCITY_TO_HAND_SCALE);
	}
}

void AbilityHand::set_torque_targets(const float torques_mNm[NUM_CHANNELS])
{
	// Convert mNm to current using motor torque constant
	// Note: This assumes a default current conversion ratio
	// In production, C should be read from the hand via Bluetooth
	for (int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		float current_amps = torques_mNm[ch] / MOTOR_TORQUE_CONSTANT;
		hand_state.iq_desired[ch] = (int16_t)(current_amps / MOTOR_IQ_CONV_CONSTANT);
	}
}

void AbilityHand::set_voltage_targets(const float voltage_percent[NUM_CHANNELS])
{
	for (int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		int16_t voltage_digital = (int16_t)(voltage_percent[ch] * MAX_VOLTAGE_DIGITAL / 100.0f);

		// Clamp to valid range
		if (voltage_digital > MAX_VOLTAGE_DIGITAL)
			voltage_digital = MAX_VOLTAGE_DIGITAL;
		if (voltage_digital < -MAX_VOLTAGE_DIGITAL)
			voltage_digital = -MAX_VOLTAGE_DIGITAL;

		hand_state.vq_desired[ch] = voltage_digital;
	}
}

void AbilityHand::get_positions(float positions_deg[NUM_CHANNELS]) const
{
	for (int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		positions_deg[ch] = hand_state.q[ch] / POSITION_SCALE;
	}
}

void AbilityHand::get_velocities(float velocities_deg_per_sec[NUM_CHANNELS]) const
{
	// Convert from rotor velocity (rad/s) to finger velocity (deg/s)
	for (int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		float rotor_velocity_rad_per_sec = hand_state.velocity[ch] / ROTOR_VELOCITY_FROM_HAND_SCALE;

		// Apply gear ratio (thumb rotator ch5 has different ratio)
		float gear_ratio = (ch == 5) ? GEAR_RATIO_THUMB_ROTATOR : GEAR_RATIO_FINGERS;

		// Convert rad/s to deg/s and account for gear ratio
		velocities_deg_per_sec[ch] = rotor_velocity_rad_per_sec * (180.0f / (float)(M_PI)) / gear_ratio;
	}
}

void AbilityHand::get_currents(float currents_amps[NUM_CHANNELS]) const
{
	// Note: This assumes a default current conversion ratio
	// In production, C should be read from the hand via Bluetooth
	for (int ch = 0; ch < NUM_CHANNELS; ch++)
	{
		currents_amps[ch] = (float)hand_state.iq[ch] * MOTOR_IQ_CONV_CONSTANT;
	}
}

void AbilityHand::get_fsr_forces(float forces_newtons[NUM_FINGERS * NUM_FSR_PER_FINGER]) const
{
	for (int i = 0; i < NUM_FINGERS * NUM_FSR_PER_FINGER; i++)
	{
		// Convert 12-bit ADC value to voltage
		float voltage = hand_state.fsr_raw[i] * FSR_VOLTAGE_SCALE;

		// Avoid division by zero
		if (voltage < 0.001f)
		{
			forces_newtons[i] = 0.0f;
			continue;
		}

		// Convert voltage to resistance
		float resistance = FSR_R_CONST1 / voltage + FSR_R_CONST2;

		// Convert resistance to force
		forces_newtons[i] = FSR_FORCE_C1 / resistance + FSR_FORCE_C2;
	}
}

AbilityHand::Statistics AbilityHand::get_statistics() const
{
	Statistics result = stats_;

	// Calculate packet loss percentage
	if (result.num_writes > 0)
	{
		result.packet_loss_percent = (1.0f - ((float)result.num_successful_reads / (float)result.num_writes)) * 100.0f;
	}
	else
	{
		result.packet_loss_percent = 0.0f;
	}

	return result;
}

void AbilityHand::reset_statistics()
{
	memset(&stats_, 0, sizeof(stats_));
}

abh_api_t& AbilityHand::get_hand_state()
{
	return hand_state;
}

Serial& AbilityHand::get_serial()
{
	return serial_;
}

UdpState& AbilityHand::get_socket()
{
	return m_socket_;
}
