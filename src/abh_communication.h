/**
 * @file abh_communication.h
 * @brief Ability Hand serial communication protocol definitions and API.
 *
 * This file defines the low-level communication protocol for the Psyonic Ability Hand,
 * including frame construction, parsing, and data structures for control and feedback.
 * 
 * These functions assume message arrives under framing: i.e. outgoing data should be 
 * byte stuffed, etc. in a second operation, and incoming data should be byte unstuffed, etc. 
 * before passing to this parsing core.
 */

#ifndef ABH_COMMUNICATION_H
#define ABH_COMMUNICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/** @brief Number of motor control channels in the Ability Hand */
#define NUM_CHANNELS 6

/** @brief Number of force-sensitive resistors (FSR) per finger */
#define NUM_FSR_PER_FINGER 6

/** @brief Number of fingers on the Ability Hand */
#define NUM_FINGERS	5

/**
 * @brief Command headers for serial communication with the Ability Hand.
 *
 * These command headers specify the control mode and reply format for frames
 * sent to the hand. The suffix (TX1, TX2, TX3) indicates the reply data format.
 */
enum
{
	FIXED_DUMMY_TX1 = 0xA0,              /**< Read only, reply mode 1 */
    FIXED_DUMMY_TX2 = 0xA1,              /**< Read only, reply mode 2 */
    FIXED_DUMMY_TX3 = 0xA2,              /**< Read only, reply mode 3 */
	FIXED_POSITION_CONTROL_TX1 = 0x10,   /**< Position control, reply mode 1 */
    FIXED_POSITION_CONTROL_TX2 = 0x11,   /**< Position control, reply mode 2 */
    FIXED_POSITION_CONTROL_TX3 = 0x12,   /**< Position control, reply mode 3 */
    FIXED_VELOCITY_CONTROL_TX1 = 0x20,   /**< Velocity control, reply mode 1 */
    FIXED_VELOCITY_CONTROL_TX2 = 0x21,   /**< Velocity control, reply mode 2 */
    FIXED_VELOCITY_CONTROL_TX3 = 0x22,   /**< Velocity control, reply mode 3 */
    FIXED_TORQUE_CONTROL_TX1 = 0x30,     /**< Torque control, reply mode 1 */
    FIXED_TORQUE_CONTROL_TX2 = 0x31,     /**< Torque control, reply mode 2 */
    FIXED_TORQUE_CONTROL_TX3 = 0x32,     /**< Torque control, reply mode 3 */
    FIXED_VOLTAGE_CONTROL_TX1 = 0x40,    /**< Voltage control, reply mode 1 */
    FIXED_VOLTAGE_CONTROL_TX2 = 0x41,    /**< Voltage control, reply mode 2 */
    FIXED_VOLTAGE_CONTROL_TX3 = 0x42,    /**< Voltage control, reply mode 3 */

	UART_WRITE_REGISTER = 0xDE,          /**< Command to write a register value */
	UART_READ_REGISTER = 0xDA,           /**< Command to read a register value */
	ENABLE_BLUETOOTH_RADIO = 0x07,       /**< Enable the Bluetooth radio */
	DISABLE_BLUETOOTH_RADIO = 0x08,      /**< Disable the Bluetooth radio */
	RESTART_HAND = 0x09,                 /**< Restart the hand controller */
	API_EXIT_CMD = 0x7C                  /**< Exit API mode */
};

/**
 * @brief Error codes and status values returned by API functions.
 *
 * Success is indicated by ABH_SUCCESS (0). All error codes are negative values
 * indicating specific failure conditions.
 */
enum
{
	ABH_SUCCESS = 0,                     /**< Operation completed successfully */
	ABH_ERROR_INVALID_ARGUMENT = -1,     /**< Invalid parameter passed to function */
	ABH_ERROR_CHECKSUM_MISMATCH = -2,    /**< Received frame checksum does not match calculated value */
	ABH_ERROR_INVALID_REPLY_MODE = -3,   /**< Reply format header does not match expected mode */
	ABH_ERROR_INVALID_HEADER = -4,       /**< Command header is not recognized */
	ABH_ERROR_BUFFER_OVERRUN = -5,       /**< Buffer size insufficient for operation */
	ABH_ERROR_NOT_CONNECTED = -6,        /**< Serial connection is not established */
	ABH_ERROR_TIMEOUT = -7,              /**< Communication timeout occurred */
	ABH_ERROR_CONNECTION_FAILED = -8,    /**< Failed to establish serial connection */
	ABH_ERROR_ALREADY_CONNECTED = -9,    /**< Connection already exists */
	ABH_ERROR_WRITE_FAILED = -10,         /**< Failed to write data to serial port */
	ABH_ERROR_INVALID_REGISTER_REPLY = 11
};

/** @brief Success code for register interface */
#define	ABH_REGISTER_SUCCESS 			0x4E11FEA4
/** @brief Fail code for register interface */
#define	ABH_REGISTER_FAIL 				0xFA17FFFF
/** @brief Register write response length */
#define ABH_REGISTER_WRITE_REPLY_LEN 	4
/** @brief Register read response length */
#define ABH_REGISTER_READ_REPLY_LEN		9

/**
 * @brief Read reply type abstraction
 */
enum 
{
	ABH_REGISTER_WRIE_REPLY,	//we have identified the reply frame as a write reply based on structure/size/value
	ABH_REGISTER_READ_REPLY,
	ABH_REGISTER_INVALID_REPLY	//the parsed message is invalid. We captured something other than a register read/write reply
};

/**
 * @brief Register read reply abstraction
 * Container to store parsed register read reply and write reply message contents
 */
typedef struct abh_register_reply_t
{
	bool request_valid;	//true if the message is signed as valid, false otherwise
	int reply_type;	//write, read, or invalid
	uint32_t register_value;	//unmodified if write reply, register read value if read reply
}abh_register_reply_t;

/**
 * @brief Main API data structure for Ability Hand communication.
 *
 * This structure contains all state information and parameters needed for
 * serial communication with the Ability Hand, including command settings,
 * desired setpoints, and sensor feedback data.
 *
 * @note TODO - eventually set up as a dartt parameter list, potentially with dartt generated headers
 */
typedef struct abh_api_t
{
	uint8_t address;                                  /**< Device address for communication */
	uint8_t command_header;                           /**< Frame type (e.g., FIXED_POSITION_CONTROL_TX1) */
	uint8_t format_header;                            /**< Reply format header indicating data structure */
	int16_t q[NUM_CHANNELS];                          /**< Actual position feedback from motors */
	int16_t q_desired[NUM_CHANNELS];                  /**< Desired position setpoints for motors */
	int16_t vq_desired[NUM_CHANNELS];                 /**< Desired voltage setpoints for motors */
	int16_t iq[NUM_CHANNELS];                         /**< Actual current feedback from motors */
	int16_t iq_desired[NUM_CHANNELS];                 /**< Desired current/torque setpoints for motors */
	int16_t velocity[NUM_CHANNELS];                   /**< Actual velocity feedback from motors */
	int16_t velocity_desired[NUM_CHANNELS];           /**< Desired velocity setpoints for motors */
	int16_t fsr_raw[NUM_FINGERS*NUM_FSR_PER_FINGER];  /**< Raw FSR sensor values (12-bit packed data) */
	uint8_t hot_cold_bitmask;                         /**< Status bitmask for motor temperature warnings */

}abh_api_t;

typedef struct abh_buffer_t
{
	unsigned char * buf;
	size_t size;
	size_t len;
}abh_buffer_t;

/**
 * @brief Calculates a generic 2's complement checksum for a byte array.
 *
 * This checksum algorithm is used throughout the Psyonic API for frame validation.
 *
 * @param arr Pointer to the byte array to checksum.
 * @param size Number of bytes to include in the checksum calculation.
 * @return The calculated 8-bit checksum value.
 */
uint8_t abh_get_checksum(uint8_t * arr, int size);

/**
 * @brief Verify checksum validity and then pop it off the tail of the message
 * 
 * This calls abh_get_checksum and returns true on match. If there is a mismatch, it returns false.
 *
 *  @param msg The message to check. Not mutated by this function.
 * 	@return True if checksum matches. False if the message does not match.
 */
bool abh_verify_checksum(abh_buffer_t * msg);

/**
 * @brief Creates a register write command frame.
 *
 * Constructs a complete serial frame to write a 32-bit value to a register
 * on the Ability Hand. The frame includes address, command header, register
 * address, value, and checksum.
 *
 * @param abh Pointer to the API state structure (used for address).
 * @param reg_address 32-bit register address to write to.
 * @param value 32-bit value to write to the register.
 * @param frame Pointer to the buffer where the frame will be stored.
 * @return ABH_SUCCESS on success, or an error code on failure.
 */
int abh_create_write_register_frame(abh_api_t * abh, uint32_t reg_address, uint32_t value, abh_buffer_t * frame);

/**
 * @brief Creates a register read command frame.
 *
 * Constructs a complete serial frame to read a 32-bit value from a register
 * on the Ability Hand. The frame includes address, command header, register
 * address, and checksum.
 *
 * @param abh Pointer to the API state structure (used for address).
 * @param reg_address 32-bit register address to read from.
 * @param frame Pointer to the buffer where the frame will be stored.
 * @return ABH_SUCCESS on success, or an error code on failure.
 */
int abh_create_read_register_frame(abh_api_t  * abh, uint32_t reg_address, abh_buffer_t * frame);

/**
 * @brief Parses a read register reply frame.
 *
 * Extracts the 32-bit register value from a reply frame received after a
 * register read command. Validates the checksum before parsing.
 *
 * @param frame Pointer to the buffer containing the received reply frame.
 * @param abh Pointer to the API state structure.
 * @param reply_word Pointer to store the extracted 32-bit register value.
 * @return ABH_SUCCESS on success, ABH_ERROR_CHECKSUM_MISMATCH if validation fails, or other error codes.
 * @note This function is currently a stub and returns 0 immediately.
 */
int abh_parse_register_reply(abh_buffer_t * frame, abh_api_t * abh, abh_register_reply_t * msg);

/**
 * @brief Creates a short API command frame.
 *
 * Constructs a frame for short API commands such as enabling/disabling Bluetooth,
 * restarting the hand, or other single-byte commands.
 *
 * @param abh Pointer to the API state structure (command_header specifies the command).
 * @param frame Pointer to the buffer where the frame will be stored.
 * @return ABH_SUCCESS on success, or an error code on failure.
 */
int abh_create_short_api_frame(abh_api_t * abh, abh_buffer_t * frame);

/**
 * @brief Creates a movement control frame.
 *
 * Constructs a complete frame for motor control with desired setpoints
 * (position, velocity, torque, or voltage depending on command_header).
 * Includes all setpoint data and checksum.
 *
 * @param abh Pointer to the API state structure containing command and setpoint data.
 * @param frame Pointer to the buffer where the frame will be stored.
 * @return ABH_SUCCESS on success, or an error code on failure.
 */
int abh_create_movement_frame(abh_api_t * abh, abh_buffer_t * frame);

/**
 * @brief Parses a movement control reply frame.
 *
 * Extracts motor feedback data (position, velocity, current, FSR values, etc.)
 * from a reply frame received after a movement command. Validates checksum
 * and reply format, then unpacks data into the API state structure.
 *
 * @param frame Pointer to the buffer containing the received reply frame.
 * @param abh Pointer to the API state structure where parsed data will be stored.
 * @return ABH_SUCCESS on success, or an error code (ABH_ERROR_CHECKSUM_MISMATCH,
 *         ABH_ERROR_INVALID_REPLY_MODE, ABH_ERROR_BUFFER_OVERRUN) on failure.
 */
int abh_parse_movement_reply(abh_buffer_t * frame, abh_api_t * abh);

#ifdef __cplusplus
}
#endif

#endif
