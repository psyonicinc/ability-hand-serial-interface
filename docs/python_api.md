# Python API Reference (`abh`)

The `abh` module exposes the full `AbilityHand` C++ API as a Python extension built with pybind11. The binary is compiled alongside the rest of the C/C++ library - no separate Python packaging step is required.

## Setup

Build with `-DBUILD_PYTHON_BINDINGS=ON` (see the main README), then add the output directory to your Python path:

```python
import sys
sys.path.append("path/to/build/python/Release")  # adjust config as needed
import abh
```

Or copy the `.pyd`/`.so` next to your script.

---

## Quick Start

```python
import abh

hand = abh.AbilityHand()
hand.init()                        # serial, autoconnect with default baud of 460800
# hand.init("COM5")					# serial - windows
# hand.init("/dev/ttyUSB0")             # serial - Linux/macOS
# hand.init_udp("192.168.1.100", 8080)  # UDP

hand.set_motion_mode(abh.MotionMode.POSITION_MODE_TX1)
hand.set_position_targets([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])

while True:
    ret = hand.exchange_motion_data()
    if ret == abh.ABH_SUCCESS:
        positions = hand.get_positions()
        print(positions)
```

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
Controls what setpoint type is sent in each motion frame.

```
POSITION_MODE_TX1 / TX2 / TX3
VELOCITY_MODE_TX1 / TX2 / TX3
TORQUE_MODE_TX1   / TX2 / TX3
VOLTAGE_MODE_TX1  / TX2 / TX3
```

TX1/TX2/TX3 select the reply data format returned by the hand.

### `ShortMode`
```
DUMMY_READ_TX1 / TX2 / TX3
```

### `CommType`
```
IFACE_SERIAL
IFACE_UDP
```

### `StuffingMode`
```
STUFFING_PPP
STUFFING_COBS
```

### `Finger`
```
INDEX = 0, MIDDLE = 1, RING = 2, PINKY = 3, THUMB_FLEXOR = 4, THUMB_ROTATOR = 5
```

### `UpsamplingChannel`
Bitmask flags for selecting which fingers to upsample.

```
UPSAMPLING_INDEX, UPSAMPLING_MIDDLE, UPSAMPLING_RING,
UPSAMPLING_PINKY, UPSAMPLING_THUMB_FLEXOR, UPSAMPLING_THUMB_ROTATOR,
UPSAMPLING_ALL
```

---

## Classes

### `AbilityHand`

Main interface class. One instance per connected hand.

#### Initialization

```python
hand.init(baud_rate=921600) -> int
hand.init(port_name: str, baud_rate=921600) -> int
hand.init_udp(ip: str, port: int) -> int
hand.init_udp(endpoint: UdpEndpoint) -> int
hand.disconnect()
hand.is_connected() -> bool
```

#### Mode Selection

```python
hand.set_motion_mode(mode: MotionMode) -> int
hand.set_short_mode(mode: ShortMode) -> int
```

Must be called before the first `exchange_motion_data` / `exchange_short_msg`.

#### Communication

```python
hand.exchange_motion_data(timeout_ms=50) -> int
hand.exchange_short_msg(timeout_ms=50) -> int
hand.command_handling(payload: str, nresponses_expected: int, timeout_ms=50) -> tuple[int, list[str]]
```

`exchange_motion_data` sends the current setpoints and populates `hand_state` with feedback. Returns `ABH_SUCCESS` or an error code.

`command_handling` returns `(return_code, responses)`.

#### Setters
All accept a list of exactly `NUM_CHANNELS` (6) floats.

```python
hand.set_position_targets(positions_deg: list[float])
hand.set_velocity_targets(velocities_deg_per_sec: list[float])
hand.set_torque_targets(torques_mNm: list[float])
hand.set_voltage_targets(voltage_percent: list[float])
```

#### Getters
All return a list of floats with engineering-unit conversions applied.

```python
hand.get_positions() -> list[float]       # degrees, length 6
hand.get_velocities() -> list[float]      # deg/s, length 6
hand.get_currents() -> list[float]        # amps, length 6
hand.get_fsr_forces() -> list[float]      # newtons, length 30 (NUM_FINGERS * NUM_FSR_PER_FINGER)
```

#### Register Access

```python
hand.read_register(addr: int, timeout_ms=50) -> tuple[int, RegisterReply]
hand.write_register(addr: int, value: int, timeout_ms=50) -> tuple[int, RegisterReply]
```

Both return `(return_code, reply)` - the out-parameter is folded into the tuple.

#### Upsampling

```python
hand.enable_upsampling(mask=UpsamplingChannel.UPSAMPLING_ALL) -> int
hand.disable_upsampling(mask=UpsamplingChannel.UPSAMPLING_ALL) -> int
```

#### Discovery (UDP)

```python
hand.discover_forwarding_servers(timeout: int) -> list[UdpEndpoint]
```

#### Statistics

```python
hand.get_statistics() -> Statistics
hand.reset_statistics()
```

#### Members

```python
hand.hand_state      # HandState - live feedback after each exchange_motion_data call
hand.stuffing_mode   # StuffingMode
```

---

### `HandState`

Populated after each `exchange_motion_data` call. All array fields return `list[int]` (raw int16 values from the hand - use the high-level getters for engineering units).

```python
state.address           # int
state.command_header    # int
state.format_header     # int
state.hot_cold_bitmask  # int - bitmask of motor temperature warnings
state.q                 # list[int], length 6 - position feedback
state.q_desired         # list[int], length 6
state.vq_desired        # list[int], length 6
state.iq                # list[int], length 6 - current feedback
state.iq_desired        # list[int], length 6
state.velocity          # list[int], length 6
state.velocity_desired  # list[int], length 6
state.fsr_raw           # list[int], length 30 - raw FSR sensor values
```

### `Statistics`

```python
stats.num_writes           # int
stats.num_successful_reads # int
stats.num_timeouts         # int
stats.num_parse_errors     # int
stats.packet_loss_percent  # float
```

### `RegisterReply`

```python
reply.request_valid   # bool
reply.reply_type      # int - ABH_REGISTER_INVALID_REPLY / WRITE_REPLY / READ_REPLY
reply.register_value  # int (uint32)
```

### `UdpEndpoint`

```python
ep = abh.UdpEndpoint()
ep.ip = "192.168.1.100"   # str
ep.port = 8080             # int
```

---

## Example: Serial Position Control Loop

```python
import abh
import time

hand = abh.AbilityHand()
ret = hand.init("COM3", 921600)
if ret != abh.ABH_SUCCESS:
    raise RuntimeError(f"Failed to connect: {ret}")

hand.set_motion_mode(abh.MotionMode.POSITION_MODE_TX1)

targets = [45.0, 45.0, 45.0, 45.0, 45.0, 0.0]  # degrees
hand.set_position_targets(targets)

try:
    while True:
        ret = hand.exchange_motion_data()
        if ret == abh.ABH_SUCCESS:
            print(hand.get_positions())
        time.sleep(0.01)
finally:
    hand.disconnect()
```

## Example: UDP Discovery and Connect

```python
import abh

hand = abh.AbilityHand()
servers = hand.discover_forwarding_servers(timeout=2000)
if not servers:
    raise RuntimeError("No servers found")

ep = servers[0]
print(f"Connecting to {ep.ip}:{ep.port}")
hand.init_udp(ep)
```
