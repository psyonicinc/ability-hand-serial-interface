#include "AbilityHand.h"
#include <cstring>
#include <cmath>
#include <chrono>

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
	m_socket = { TCS_NULLSOCKET, "192.168.1.100", 5000, false };
}

AbilityHand::~AbilityHand()
{
    disconnect();
	udp_disconnect(&m_socket);
}

std::vector<UdpEndpoint> AbilityHand::discover_forwarding_servers(uint32_t timeout)
{
    std::vector<UdpEndpoint> results;

    tcs_lib_init();

    TcsSocket sock = TCS_NULLSOCKET;
    TcsReturnCode res = tcs_create(&sock, TCS_TYPE_UDP_IP4);
    if (res != TCS_SUCCESS)
    {
        tcs_lib_free();
        return results;
    }

    tcs_set_broadcast(sock, true);

    struct TcsAddress local_addr = TCS_ADDRESS_NULL;
    local_addr.family = TCS_AF_IP4;
    local_addr.data.af_inet.address = 0;
    local_addr.data.af_inet.port = 0;
    tcs_bind_address(sock, &local_addr);

    // Derive directed broadcast from first non-loopback local IPv4 address
    uint32_t bcast_ip = TCS_ADDRESS_BROADCAST_IP4;  // fallback: 255.255.255.255
    struct TcsInterface ifaces[16];
    size_t iface_count = 0;
    if (tcs_local_interfaces(ifaces, 16, &iface_count) == TCS_SUCCESS)
    {
        for (size_t i = 0; i < iface_count; i++)
        {
            if (ifaces[i].address.family != TCS_AF_IP4)
                continue;
            uint32_t addr = ifaces[i].address.data.af_inet.address;
            if ((addr >> 24) == 127)  // skip loopback (127.x.x.x)
                continue;
            bcast_ip = (addr & 0xFFFFFF00) | 0xFF;
            break;
        }
    }

    struct TcsAddress bcast_addr = TCS_ADDRESS_NULL;
    bcast_addr.family = TCS_AF_IP4;
    bcast_addr.data.af_inet.address = bcast_ip;
    bcast_addr.data.af_inet.port = 5067;

    size_t sent = 0;
    res = tcs_send_to(sock, (const uint8_t*)"WHO_GOES_THERE", 14, TCS_NO_FLAGS, &bcast_addr, &sent);
	if(res != TCS_SUCCESS)
	{
		tcs_destroy(&sock);
        tcs_lib_free();
		return results;	//empty due to failed send
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
	init_buffers();
	for(int i = 0; i < sizeof(m_socket.ip); i++)
	{
		m_socket.ip[i] = 0;
	}
	int retl = snprintf(m_socket.ip, sizeof(m_socket.ip), "%s", ip);		//consider checking returned size to avoid issues related to null termination
	if(retl >= sizeof(m_socket.ip))
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	bool is_null_terminated = false;
	for(int i = 0; i < sizeof(m_socket.ip); i++)
	{
		if(m_socket.ip[i] == 0)
		{
			is_null_terminated = true;
			break;
		}
	}
	if(is_null_terminated == false)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}

	m_socket.port = port;
	bool success = udp_connect(&m_socket);
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

    init_buffers();
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

    init_buffers();
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
		return m_socket.connected;
	}
}

void AbilityHand::init_buffers()
{
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

    // Initialize RX buffers
    rx_buffer_alias_.buf = rx_stuffed_mem_;
    rx_buffer_alias_.size = sizeof(rx_stuffed_mem_);
    rx_buffer_alias_.length = 0;

    rx_ppp_unstuff_alias_.buf = rx_unstuff_mem_;
    rx_ppp_unstuff_alias_.size = sizeof(rx_unstuff_mem_);
    rx_ppp_unstuff_alias_.length = 0;

    rx_frame_.buf = rx_unstuff_mem_;
    rx_frame_.size = sizeof(rx_unstuff_mem_);
    rx_frame_.len = 0;
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

int AbilityHand::exchange_motion_data(int timeout_ms)
{
    if (!is_connected())
    {
        return ABH_ERROR_NOT_CONNECTED;
    }
	
    // Create movement frame
    int rc = abh_create_movement_frame(&hand_state, &tx_frame_);
    if (rc != ABH_SUCCESS)
    {
        return rc;
    }

    return send_and_receive(&tx_frame_, timeout_ms);
}

int AbilityHand::exchange_short_msg(int timeout_ms)
{
    if (!is_connected())
    {
        return ABH_ERROR_NOT_CONNECTED;
    }

    // Create short API frame
    int rc = abh_create_short_api_frame(&hand_state, &tx_frame_);
    if (rc != ABH_SUCCESS)
    {
        return rc;
    }

    return send_and_receive(&tx_frame_, timeout_ms);
}
int AbilityHand::send_recieve_udp(abh_buffer_t * tx_frame, int timeout_ms)
{
	// Stuff the frame
    abh_to_ppp(tx_frame, &tx_unstuff_alias_);
	PPP_stuff(&tx_unstuff_alias_, &tx_stuff_buf_);
	if (!m_socket.connected)
	{
		return ABH_ERROR_NOT_CONNECTED;
	}
	size_t bytes_sent = 0;
	TcsReturnCode res = tcs_send(m_socket.socket, tx_stuff_buf_.buf, tx_stuff_buf_.length, TCS_NO_FLAGS, &bytes_sent);
	if(res == TCS_SUCCESS && bytes_sent == tx_stuff_buf_.length)
	{
		stats_.num_writes++;
	}
	else
	{
		return ABH_ERROR_WRITE_FAILED;
	}

	struct TcsAddress src;
	size_t bytes_received = 0;
	tcs_set_receive_timeout(m_socket.socket, timeout_ms);
	res = tcs_receive_from(m_socket.socket, rx_buffer_alias_.buf, rx_buffer_alias_.size, TCS_NO_FLAGS, &src, &bytes_received);
	if(res == TCS_SUCCESS)
	{
		rx_buffer_alias_.length = bytes_received;
	}
	else
	{
		rx_buffer_alias_.length = 0;
		return ABH_ERROR_TIMEOUT;
	}
	if(bytes_received == 0)
	{
		return ABH_ERROR_INVALID_ARGUMENT;
	}
	PPP_unstuff(&rx_ppp_unstuff_alias_, &rx_buffer_alias_);

	// Parse the frame
    rx_frame_.len = rx_ppp_unstuff_alias_.length;
    int rc = ahb_parse_movement_reply(&rx_frame_, &hand_state);
    if (rc != ABH_SUCCESS)
    {
        stats_.num_parse_errors++;
        return rc;
    }

    // Success
    stats_.num_successful_reads++;
    return ABH_SUCCESS;
}

int AbilityHand::send_recieve_serial(abh_buffer_t * tx_frame, int timeout_ms)
{
	    // Stuff the frame
    abh_to_ppp(tx_frame, &tx_unstuff_alias_);
	PPP_stuff(&tx_unstuff_alias_, &tx_stuff_buf_);


    // Send the stuffed frame
    int bytes_written = serial_.write(tx_stuff_buf_.buf, static_cast<int>(tx_stuff_buf_.length));
    stats_.num_writes++;

    if (bytes_written != (int)tx_stuff_buf_.length)
    {
        return ABH_ERROR_WRITE_FAILED;
    }

    // Read response
    int rxlen = serial_.read_dual_delimiter(rx_buffer_alias_.buf, (int)rx_buffer_alias_.size, '~', timeout_ms);

    if (rxlen == -2)
    {
        // Timeout
        stats_.num_timeouts++;
        return ABH_ERROR_TIMEOUT;
    }

    if (rxlen <= 0)
    {
        return ABH_ERROR_INVALID_ARGUMENT;
    }

    // Unstuff the frame
    rx_buffer_alias_.length = rxlen;
    PPP_unstuff(&rx_ppp_unstuff_alias_, &rx_buffer_alias_);

    // Parse the frame
    rx_frame_.len = rx_ppp_unstuff_alias_.length;
    int rc = ahb_parse_movement_reply(&rx_frame_, &hand_state);
    if (rc != ABH_SUCCESS)
    {
        stats_.num_parse_errors++;
        return rc;
    }

    // Success
    stats_.num_successful_reads++;
    return ABH_SUCCESS;
}

int AbilityHand::send_and_receive(abh_buffer_t* tx_frame, int timeout_ms)
{
	if(iface_ == IFACE_SERIAL)
	{
		return send_recieve_serial(tx_frame, timeout_ms);
	}
	else
	{
		return send_recieve_udp(tx_frame, timeout_ms);
	}
}

void AbilityHand::abh_to_ppp(abh_buffer_t* in, ppp_buffer_t* out)
{
    out->buf = in->buf;
    out->length = in->len;
    out->size = in->size;
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
        velocities_deg_per_sec[ch] = rotor_velocity_rad_per_sec * (180.0f / static_cast<float>(M_PI)) / gear_ratio;
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
