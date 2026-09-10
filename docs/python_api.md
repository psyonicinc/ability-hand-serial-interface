# Python API Reference (`abh`)

The `abh` module exposes the full `AbilityHand` C++ API as a Python extension built with pybind11. See `examples/python/` for runnable usage examples.

## Setup

Build with `-DBUILD_PYTHON_BINDINGS=ON` (see the main README), then add the output directory to your Python path, or install via pip (see README for venv/pip instructions).

---

## Constants

### Dimensions
| Name | Value | Description |
|---|---|---|
| `NUM_CHANNELS` | 6 | Number of motor control channels |
| `NUM_FINGERS` | 5 | Number of fingers |
| `NUM_FSR_PER_FINGER` | 6 | FSR sensors per finger |

### Error Codes
All functions that can fail return one of these `int` values.

| Name | Value |
|---|---|
| `ABH_SUCCESS` | 0 |
| `ABH_ERROR_INVALID_ARGUMENT` | -1 |
| `ABH_ERROR_CHECKSUM_MISMATCH` | -2 |
| `ABH_ERROR_INVALID_REPLY_MODE` | -3 |
| `ABH_ERROR_INVALID_HEADER` | -4 |
| `ABH_ERROR_BUFFER_OVERRUN` | -5 |
| `ABH_ERROR_NOT_CONNECTED` | -6 |
| `ABH_ERROR_TIMEOUT` | -7 |
| `ABH_ERROR_CONNECTION_FAILED` | -8 |
| `ABH_ERROR_ALREADY_CONNECTED` | -9 |
| `ABH_ERROR_WRITE_FAILED` | -10 |
| `ABH_ERROR_INVALID_REGISTER_REPLY` | -11 |
| `ABH_ERROR_REPLY_LEN_INVALID` | -12 |

---

## Enums

### `MotionMode`
Selects the control mode and reply format for motion frames. TX1/TX2/TX3 select the reply data format returned by the hand.

| Value |
|---|
| `POSITION_MODE_TX1` / `TX2` / `TX3` |
| `VELOCITY_MODE_TX1` / `TX2` / `TX3` |
| `TORQUE_MODE_TX1` / `TX2` / `TX3` |
| `VOLTAGE_MODE_TX1` / `TX2` / `TX3` |

### `ShortMode`
| Value |
|---|
| `DUMMY_READ_TX1` / `TX2` / `TX3` |

### `CommType`
| Value |
|---|
| `IFACE_SERIAL` |
| `IFACE_UDP` |

### `StuffingMode`
| Value |
|---|
| `STUFFING_PPP` |
| `STUFFING_COBS` |

### `Finger`
| Value | Index |
|---|---|
| `INDEX` | 0 |
| `MIDDLE` | 1 |
| `RING` | 2 |
| `PINKY` | 3 |
| `THUMB_FLEXOR` | 4 |
| `THUMB_ROTATOR` | 5 |

### `UpsamplingChannel`
Bitmask flags for selecting upsampled channels.

| Value |
|---|
| `UPSAMPLING_INDEX` |
| `UPSAMPLING_MIDDLE` |
| `UPSAMPLING_RING` |
| `UPSAMPLING_PINKY` |
| `UPSAMPLING_THUMB_FLEXOR` |
| `UPSAMPLING_THUMB_ROTATOR` |
| `UPSAMPLING_ALL` |

### `GripCommand`
Selects a preset grip pattern for `exchange_grip_data`.

| Value |
|---|
| `GRIP_OPEN` |
| `GRIP_POWER` |
| `GRIP_KEY` |
| `GRIP_PINCH` |
| `GRIP_TRIPOD_OPENED` |
| `GRIP_SIGN_OF_THE_HORNS` |
| `GRIP_CYLINDER` |
| `GRIP_MOUSE` |
| `GRIP_POWER_KEY_SWITCH` |
| `GRIP_POINT` |
| `GRIP_RUDE_POINT` |
| `GRIP_HOOK` |
| `GRIP_RELAX` |
| `GRIP_SLEEVE` |
| `GRIP_PEACE` |
| `GRIP_TRIPOD_CLOSED` |
| `GRIP_HANGLOOSE` |
| `GRIP_HANDSHAKE` |
| `GRIP_FIXED_PINCH` |
| `GRIP_USER_7` / `8` / `9` / `10` / `11` |
| `GRIP_TRIGGER` |
| `GRIP_USER_12` / `13` / `14` / `15` / `16` / `17` / `18` |
| `GRIP_FINGER_WAVE` |

---

## Classes

### `AbilityHand`

Main interface class. One instance per connected hand.

#### Initialization

| Method | Returns | Description |
|---|---|---|
| `init(baud_rate=921600)` | `int` | Connect to first available serial port |
| `init(port_name, baud_rate=921600)` | `int` | Connect to named serial port |
| `init_udp(ip, port)` | `int` | Connect via UDP |
| `init_udp(endpoint)` | `int` | Connect via UDP using a `UdpEndpoint` |
| `discover_forwarding_servers(timeout)` | `list[UdpEndpoint]` | Discover UDP forwarding servers |
| `disconnect()` | `None` | Close the connection |
| `is_connected()` | `bool` | Returns connection state |

#### Mode Selection

Must be called before the first `exchange_motion_data` or `exchange_short_msg`.

| Method | Returns | Description |
|---|---|---|
| `set_motion_mode(mode)` | `int` | Set motion control mode |
| `set_short_mode(mode)` | `int` | Set short message mode |

#### Communication

| Method | Returns | Description |
|---|---|---|
| `exchange_motion_data(timeout_ms=50)` | `int` | Send setpoints, receive feedback into `hand_state` |
| `exchange_short_msg(timeout_ms=50)` | `int` | Send a short read-only frame |
| `command_handling(payload, nresponses_expected, timeout_ms=50)` | `tuple[int, list[str]]` | Send a command string, returns `(return_code, responses)` |

#### Setters
All accept a `list[float]` of exactly `NUM_CHANNELS` (6) values.

| Method | Units |
|---|---|
| `set_position_targets(positions_deg)` | degrees |
| `set_velocity_targets(velocities_deg_per_sec)` | degrees/second |
| `set_torque_targets(torques_mNm)` | mNm |
| `set_voltage_targets(voltage_percent)` | percent |

#### Getters
All return a `list[float]` with engineering-unit conversion applied.

| Method | Returns | Units |
|---|---|---|
| `get_positions()` | `list[float]`, length 6 | degrees |
| `get_velocities()` | `list[float]`, length 6 | degrees/second |
| `get_currents()` | `list[float]`, length 6 | amps |
| `get_fsr_forces()` | `list[float]`, length 30 | newtons |

#### Register Access
Both return `(return_code, reply)` — the out-parameter is folded into the tuple.

| Method | Returns |
|---|---|
| `read_register(addr, timeout_ms=50)` | `tuple[int, RegisterReply]` |
| `write_register(addr, value, timeout_ms=50)` | `tuple[int, RegisterReply]` |

#### Upsampling

| Method | Returns |
|---|---|
| `enable_upsampling(mask=UPSAMPLING_ALL)` | `int` |
| `disable_upsampling(mask=UPSAMPLING_ALL)` | `int` |

#### Grip Command
Returns `(return_code, reply)` — the out-parameter is folded into the tuple. 

| Method | Returns |
|---|---|
| `exchange_grip_data(grip, speed, timeout_ms=50)` | `tuple[int, GripReply]` |

#### Discovery (UDP)

| Method | Returns |
|---|---|
| `discover_forwarding_servers(timeout)` | `list[UdpEndpoint]` |

#### Statistics

| Method | Returns |
|---|---|
| `get_statistics()` | `Statistics` |
| `reset_statistics()` | `None` |

#### Members

| Member | Type | Description |
|---|---|---|
| `hand_state` | `HandState` | Live feedback populated after each `exchange_motion_data` call |
| `stuffing_mode` | `StuffingMode` | Active byte stuffing mode |

---

### `HandState`

Feedback data populated after each `exchange_motion_data` call. Array fields return raw int16 values — use the high-level getters on `AbilityHand` for engineering units.

| Field | Type | Description |
|---|---|---|
| `address` | `int` | Device address |
| `command_header` | `int` | Active command header |
| `format_header` | `int` | Reply format header |
| `hot_cold_bitmask` | `int` | Motor temperature warning bitmask |
| `q` | `list[int]`, length 6 | Position feedback |
| `q_desired` | `list[int]`, length 6 | Position setpoints |
| `vq_desired` | `list[int]`, length 6 | Voltage setpoints |
| `iq` | `list[int]`, length 6 | Current feedback |
| `iq_desired` | `list[int]`, length 6 | Current/torque setpoints |
| `velocity` | `list[int]`, length 6 | Velocity feedback |
| `velocity_desired` | `list[int]`, length 6 | Velocity setpoints |
| `fsr_raw` | `list[int]`, length 30 | Raw FSR sensor values |

---

### `Statistics`

| Field | Type |
|---|---|
| `num_writes` | `int` |
| `num_successful_reads` | `int` |
| `num_timeouts` | `int` |
| `num_parse_errors` | `int` |
| `packet_loss_percent` | `float` |

---

### `RegisterReply`

| Field | Type | Description |
|---|---|---|
| `request_valid` | `bool` | True if the reply is valid |
| `reply_type` | `int` | `ABH_REGISTER_INVALID_REPLY`, `WRITE_REPLY`, or `READ_REPLY` |
| `register_value` | `int` | Register value (read replies only) |

---

### `GripReply`

| Field | Type | Description |
|---|---|---|
| `address` | `int` | Device address |
| `vibration_motor_intensity` | `int` | Touch sensor value mapping |
| `handshake_valid` | `bool` | True if the reply's 0xBEEF handshake matched |
| `grip_color` | `list[int]`, length 3 | R, G, B grip color |
| `grip_status` | `int` | Status indicator for motion completion |
| `grip_percent` | `int` | Grip completion percentage |
| `idle` | `bool` | Motor driver idle status |

---

### `UdpEndpoint`

| Field | Type |
|---|---|
| `ip` | `str` |
| `port` | `int` |
