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
#include <cstdint>
#include <vector>

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
    int init(unsigned long baud_rate = 921600);
    int init(const char* port_name, unsigned long baud_rate = 921600);
	int init_udp(const char * ip, uint16_t port);
	int init_udp(const UdpEndpoint& endpoint);
	std::vector<UdpEndpoint> discover_forwarding_servers(uint32_t timeout);
    void disconnect();
    bool is_connected() const;

    // Mode setting
    int set_motion_mode(MotionMode mode);
    int set_short_mode(ShortMode mode);

    // Core communication
    int exchange_motion_data(int timeout_ms = 5);
    int exchange_short_msg(int timeout_ms = 5);

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

	UdpState m_socket;

private:
    // Serial communication
    Serial serial_;
	
	CommType iface_;

    // TX buffers (unstuffed → stuffed)
    unsigned char tx_mem_[128];
    abh_buffer_t tx_frame_;
    ppp_buffer_t tx_unstuff_alias_;

    unsigned char tx_stuffed_mem_[258];  // 128*2 + 2 delimiters
    ppp_buffer_t tx_stuff_buf_;

    // RX buffers (stuffed → unstuffed)
    unsigned char rx_stuffed_mem_[256];
    ppp_buffer_t rx_buffer_alias_;

    unsigned char rx_unstuff_mem_[128];
    ppp_buffer_t rx_ppp_unstuff_alias_;
    abh_buffer_t rx_frame_;

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
    void init_buffers();
    int send_and_receive(abh_buffer_t* tx_frame, int timeout_ms);
    void abh_to_ppp(abh_buffer_t* in, ppp_buffer_t* out);

	int send_recieve_serial(abh_buffer_t * tx_frame, int timeout_ms);	//internal helper for serial comms only
	int send_recieve_udp(abh_buffer_t * tx_frame, int timeout_ms);
};

#endif // ABILITY_HAND_H
