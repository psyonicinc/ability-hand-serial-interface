#ifndef ABILITY_HAND_H
#define ABILITY_HAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "abh_communication.h"

#ifdef __cplusplus
}
#endif

#include "udp.h"
#include "serial.h"
#include "PPP.h"
#include "cobs.h"
#include <cstdint>
#include <vector>
#include <chrono>

// Define M_PI for Windows
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class AbilityHand {
public:
    // Motion control modes (for movement frames)
    enum MotionMode {
        POSITION_MODE_TX1,
        POSITION_MODE_TX2,
        POSITION_MODE_TX3,
        VELOCITY_MODE_TX1,
        VELOCITY_MODE_TX2,
        VELOCITY_MODE_TX3,
        TORQUE_MODE_TX1,
        TORQUE_MODE_TX2,
        TORQUE_MODE_TX3,
        VOLTAGE_MODE_TX1,
        VOLTAGE_MODE_TX2,
        VOLTAGE_MODE_TX3
    };

    // Short message modes (for 3-byte frames)
    enum ShortMode {
        DUMMY_READ_TX1,
        DUMMY_READ_TX2,
        DUMMY_READ_TX3
    };

	enum CommType {
		IFACE_SERIAL,
		IFACE_UDP
	};

	enum StuffingMode {
		STUFFING_PPP,
		STUFFING_COBS
	};

	enum Finger : int {
		INDEX = 0,
		MIDDLE = 1,
		RING = 2, 
		PINKY = 3,
		THUMB_FLEXOR = 4,
		THUMB_ROTATOR = 5
	};

	enum UpsamplingChannel : uint32_t {
		UPSAMPLING_INDEX         = (1 << INDEX),
		UPSAMPLING_MIDDLE        = (1 << MIDDLE),
		UPSAMPLING_RING          = (1 << RING),
		UPSAMPLING_PINKY         = (1 << PINKY),
		UPSAMPLING_THUMB_FLEXOR  = (1 << THUMB_FLEXOR),
		UPSAMPLING_THUMB_ROTATOR = (1 << THUMB_ROTATOR),
		UPSAMPLING_ALL           = 0x3F
	};

    // Statistics structure
    struct Statistics {
        uint32_t num_writes;
        uint32_t num_successful_reads;
        uint32_t num_timeouts;
        uint32_t num_parse_errors;
        float packet_loss_percent;
    };

    // Constructor and destructor
    AbilityHand();
    ~AbilityHand();

    // Initialization
    int init(unsigned long baud_rate = 460800);
    int init(const char* port_name, unsigned long baud_rate = 460800);
	int init_udp(const char * ip, uint16_t port);
	int init_udp(const UdpEndpoint& endpoint);
	std::vector<UdpEndpoint> discover_forwarding_servers(uint32_t timeout);
    void disconnect();
    bool is_connected() const;

    // Mode setting
    int set_motion_mode(MotionMode mode);
    int set_short_mode(ShortMode mode);

	int read_register(uint32_t addr, abh_register_reply_t& reply, int timeout_ms = 50);
	int write_register(uint32_t addr, uint32_t value, abh_register_reply_t& reply, int timeout_ms = 50);
	int enable_upsampling(UpsamplingChannel mask = UPSAMPLING_ALL);
	int disable_upsampling(UpsamplingChannel mask = UPSAMPLING_ALL);

    // Core communication
    int exchange_motion_data(int timeout_ms = 50);
    int exchange_short_msg(int timeout_ms = 50);
    int command_handling(const std::string& payload,
                         std::vector<std::string>& responses,
                         int nresponses_expected, int timeout_ms = 50);

    // High-level setters (float-based with automatic unit conversion)
    void set_position_targets(const float positions_deg[NUM_CHANNELS]);
    void set_velocity_targets(const float velocities_deg_per_sec[NUM_CHANNELS]);
    void set_torque_targets(const float torques_mNm[NUM_CHANNELS]);
    void set_voltage_targets(const float voltage_percent[NUM_CHANNELS]);

    // High-level getters (float-based with automatic unit conversion)
    void get_positions(float positions_deg[NUM_CHANNELS]) const;
    void get_velocities(float velocities_deg_per_sec[NUM_CHANNELS]) const;
    void get_currents(float currents_amps[NUM_CHANNELS]) const;
    void get_fsr_forces(float forces_newtons[NUM_FINGERS * NUM_FSR_PER_FINGER]) const;

    // Statistics
    Statistics get_statistics() const;
    void reset_statistics();

    // Direct access to hand state (for advanced users)
    abh_api_t& get_hand_state();

    // Hand state
    abh_api_t hand_state;
	StuffingMode stuffing_mode;
	abh_buffer_t rx_frame;

	UdpState m_socket;

private:
    // Serial communication
    Serial serial_;
	
	//second copy of the callback and context used by serial_ - we re-use the parser in other comm modes like udp. 
	//could also make these public and part of serial, but pointers are very cheap, so not a big deal. Use register callback 
	//as primary entrypoint to prevent code re-use
	Serial::frame_cb_t _abh_frame_parser_cb;
	void * _abh_parser_context; 
	int register_callback(Serial::frame_cb_t cb, void * user_context = NULL);


	CommType iface_;

    // TX buffers (unstuffed → stuffed)
    unsigned char tx_mem_[128];
    abh_buffer_t tx_frame_;
    ppp_buffer_t tx_unstuff_alias_;

    unsigned char tx_stuffed_mem_[258];  // 128*2 + 2 delimiters
    ppp_buffer_t tx_stuff_buf_;
    cobs_buf_t tx_cobs_buf_;             // aliases tx_stuffed_mem_ for COBS encode

    // RX buffers (stuffed → unstuffed)
    unsigned char rx_stuffed_mem_[256];
    ppp_buffer_t rx_buffer_alias_;
    cobs_buf_t rx_cobs_encoded_;         // aliases rx_stuffed_mem_ for COBS decode

    unsigned char rx_unstuff_mem_[128];
    ppp_buffer_t rx_ppp_unstuff_alias_;
    cobs_buf_t rx_cobs_decoded_;         // aliases rx_unstuff_mem_ for COBS decode
    

    // Statistics tracking
    Statistics stats_;

    // Conversion constants from ICD
    static constexpr float POSITION_SCALE = 32767.0f / 150.0f;
    static constexpr float VELOCITY_TO_HAND_SCALE = 32767.0f / 3000.0f;
    static constexpr float ROTOR_VELOCITY_FROM_HAND_SCALE = 4.0f;
    static constexpr int16_t MAX_VOLTAGE_DIGITAL = 3546;
    static constexpr float MOTOR_TORQUE_CONSTANT = 1.49f;  // mNm/A
	static constexpr float MOTOR_IQ_CONV_CONSTANT = 3300.f/(5.f*100*4096.f);	//fixed-point representation of current to floating point (Amps)
    static constexpr float GEAR_RATIO_FINGERS = 649.0f;
    static constexpr float GEAR_RATIO_THUMB_ROTATOR = 162.45f;
    static constexpr float FSR_VOLTAGE_SCALE = 3.3f / 4096.0f;
    static constexpr float FSR_R_CONST1 = 33000.0f;
    static constexpr float FSR_R_CONST2 = 10000.0f;
    static constexpr float FSR_FORCE_C1 = 121591.0f;
    static constexpr float FSR_FORCE_C2 = 0.878894f;

    // Helper methods
    void abh_to_ppp(abh_buffer_t* in, ppp_buffer_t* out);
    void abh_to_cobs(abh_buffer_t* in, cobs_buf_t* out);

	int stuff_and_write_serial();
	int stuff_and_write_udp();
	int read_and_parse_serial(int timeout_ms);
	int read_and_parse_udp(int timeout_ms);
	int stuff_and_write();					// dispatches on iface_
	int read_and_parse(int timeout_ms);	// dispatches on iface_

	std::chrono::steady_clock::time_point timestamp_;
	void start_time();
	int64_t get_ms() const;
};

#endif // ABILITY_HAND_H
