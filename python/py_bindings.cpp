#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "AbilityHand.h"
#include "abh_communication.h"

namespace py = pybind11;

PYBIND11_MODULE(abh, m) {

    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------
    m.attr("NUM_CHANNELS")       = NUM_CHANNELS;
    m.attr("NUM_FINGERS")        = NUM_FINGERS;
    m.attr("NUM_FSR_PER_FINGER") = NUM_FSR_PER_FINGER;

    // Error codes — explicit int cast required for anonymous C enum values
    m.attr("ABH_SUCCESS")                      = (int)ABH_SUCCESS;
    m.attr("ABH_ERROR_INVALID_ARGUMENT")       = (int)ABH_ERROR_INVALID_ARGUMENT;
    m.attr("ABH_ERROR_CHECKSUM_MISMATCH")      = (int)ABH_ERROR_CHECKSUM_MISMATCH;
    m.attr("ABH_ERROR_INVALID_REPLY_MODE")     = (int)ABH_ERROR_INVALID_REPLY_MODE;
    m.attr("ABH_ERROR_INVALID_HEADER")         = (int)ABH_ERROR_INVALID_HEADER;
    m.attr("ABH_ERROR_BUFFER_OVERRUN")         = (int)ABH_ERROR_BUFFER_OVERRUN;
    m.attr("ABH_ERROR_NOT_CONNECTED")          = (int)ABH_ERROR_NOT_CONNECTED;
    m.attr("ABH_ERROR_TIMEOUT")                = (int)ABH_ERROR_TIMEOUT;
    m.attr("ABH_ERROR_CONNECTION_FAILED")      = (int)ABH_ERROR_CONNECTION_FAILED;
    m.attr("ABH_ERROR_ALREADY_CONNECTED")      = (int)ABH_ERROR_ALREADY_CONNECTED;
    m.attr("ABH_ERROR_WRITE_FAILED")           = (int)ABH_ERROR_WRITE_FAILED;
    m.attr("ABH_ERROR_INVALID_REGISTER_REPLY") = (int)ABH_ERROR_INVALID_REGISTER_REPLY;
    m.attr("ABH_ERROR_REPLY_LEN_INVALID")      = (int)ABH_ERROR_REPLY_LEN_INVALID;

    // Register reply type constants
    m.attr("ABH_REGISTER_INVALID_REPLY") = (int)ABH_REGISTER_INVALID_REPLY;
    m.attr("ABH_REGISTER_WRITE_REPLY")   = (int)ABH_REGISTER_WRITE_REPLY;
    m.attr("ABH_REGISTER_READ_REPLY")    = (int)ABH_REGISTER_READ_REPLY;

    // Register magic values
    m.attr("ABH_REGISTER_SUCCESS") = (uint32_t)ABH_REGISTER_SUCCESS;
    m.attr("ABH_REGISTER_FAIL")    = (uint32_t)ABH_REGISTER_FAIL;

    // -------------------------------------------------------------------------
    // abh_register_reply_t
    // -------------------------------------------------------------------------
    py::class_<abh_register_reply_t>(m, "RegisterReply")
        .def(py::init<>())
        .def_readwrite("request_valid",   &abh_register_reply_t::request_valid)
        .def_readwrite("reply_type",      &abh_register_reply_t::reply_type)
        .def_readwrite("register_value",  &abh_register_reply_t::register_value);

    // -------------------------------------------------------------------------
    // UdpEndpoint
    // -------------------------------------------------------------------------
    py::class_<UdpEndpoint>(m, "UdpEndpoint")
        .def(py::init<>())
        .def_property("ip",
            [](const UdpEndpoint& e) { return std::string(e.ip); },
            [](UdpEndpoint& e, const std::string& s) {
                strncpy(e.ip, s.c_str(), sizeof(e.ip) - 1);
                e.ip[sizeof(e.ip) - 1] = '\0';
            })
        .def_readwrite("port", &UdpEndpoint::port);

    // -------------------------------------------------------------------------
    // AbilityHand::Statistics
    // -------------------------------------------------------------------------
    py::class_<AbilityHand::Statistics>(m, "Statistics")
        .def(py::init<>())
        .def_readwrite("num_writes",           &AbilityHand::Statistics::num_writes)
        .def_readwrite("num_successful_reads", &AbilityHand::Statistics::num_successful_reads)
        .def_readwrite("num_timeouts",         &AbilityHand::Statistics::num_timeouts)
        .def_readwrite("num_parse_errors",     &AbilityHand::Statistics::num_parse_errors)
        .def_readwrite("packet_loss_percent",  &AbilityHand::Statistics::packet_loss_percent);

    // -------------------------------------------------------------------------
    // abh_api_t (HandState) — C array members exposed as lists via properties
    // -------------------------------------------------------------------------
    py::class_<abh_api_t>(m, "HandState")
        .def(py::init<>())
        .def_readwrite("address",        &abh_api_t::address)
        .def_readwrite("command_header", &abh_api_t::command_header)
        .def_readwrite("format_header",  &abh_api_t::format_header)
        .def_readwrite("hot_cold_bitmask", &abh_api_t::hot_cold_bitmask)
        .def_property_readonly("q", [](const abh_api_t& s) {
            return std::vector<int16_t>(s.q, s.q + NUM_CHANNELS);
        })
        .def_property_readonly("q_desired", [](const abh_api_t& s) {
            return std::vector<int16_t>(s.q_desired, s.q_desired + NUM_CHANNELS);
        })
        .def_property_readonly("vq_desired", [](const abh_api_t& s) {
            return std::vector<int16_t>(s.vq_desired, s.vq_desired + NUM_CHANNELS);
        })
        .def_property_readonly("iq", [](const abh_api_t& s) {
            return std::vector<int16_t>(s.iq, s.iq + NUM_CHANNELS);
        })
        .def_property_readonly("iq_desired", [](const abh_api_t& s) {
            return std::vector<int16_t>(s.iq_desired, s.iq_desired + NUM_CHANNELS);
        })
        .def_property_readonly("velocity", [](const abh_api_t& s) {
            return std::vector<int16_t>(s.velocity, s.velocity + NUM_CHANNELS);
        })
        .def_property_readonly("velocity_desired", [](const abh_api_t& s) {
            return std::vector<int16_t>(s.velocity_desired, s.velocity_desired + NUM_CHANNELS);
        })
        .def_property_readonly("fsr_raw", [](const abh_api_t& s) {
            return std::vector<int16_t>(s.fsr_raw, s.fsr_raw + NUM_FINGERS * NUM_FSR_PER_FINGER);
        });

    // -------------------------------------------------------------------------
    // AbilityHand enums
    // -------------------------------------------------------------------------
    py::enum_<AbilityHand::MotionMode>(m, "MotionMode")
        .value("POSITION_MODE_TX1", AbilityHand::POSITION_MODE_TX1)
        .value("POSITION_MODE_TX2", AbilityHand::POSITION_MODE_TX2)
        .value("POSITION_MODE_TX3", AbilityHand::POSITION_MODE_TX3)
        .value("VELOCITY_MODE_TX1", AbilityHand::VELOCITY_MODE_TX1)
        .value("VELOCITY_MODE_TX2", AbilityHand::VELOCITY_MODE_TX2)
        .value("VELOCITY_MODE_TX3", AbilityHand::VELOCITY_MODE_TX3)
        .value("TORQUE_MODE_TX1",   AbilityHand::TORQUE_MODE_TX1)
        .value("TORQUE_MODE_TX2",   AbilityHand::TORQUE_MODE_TX2)
        .value("TORQUE_MODE_TX3",   AbilityHand::TORQUE_MODE_TX3)
        .value("VOLTAGE_MODE_TX1",  AbilityHand::VOLTAGE_MODE_TX1)
        .value("VOLTAGE_MODE_TX2",  AbilityHand::VOLTAGE_MODE_TX2)
        .value("VOLTAGE_MODE_TX3",  AbilityHand::VOLTAGE_MODE_TX3)
        .export_values();

    py::enum_<AbilityHand::ShortMode>(m, "ShortMode")
        .value("DUMMY_READ_TX1", AbilityHand::DUMMY_READ_TX1)
        .value("DUMMY_READ_TX2", AbilityHand::DUMMY_READ_TX2)
        .value("DUMMY_READ_TX3", AbilityHand::DUMMY_READ_TX3)
        .export_values();

    py::enum_<AbilityHand::CommType>(m, "CommType")
        .value("IFACE_SERIAL", AbilityHand::IFACE_SERIAL)
        .value("IFACE_UDP",    AbilityHand::IFACE_UDP)
        .export_values();

    py::enum_<AbilityHand::StuffingMode>(m, "StuffingMode")
        .value("STUFFING_PPP",  AbilityHand::STUFFING_PPP)
        .value("STUFFING_COBS", AbilityHand::STUFFING_COBS)
        .export_values();

    py::enum_<AbilityHand::Finger>(m, "Finger")
        .value("INDEX",          AbilityHand::INDEX)
        .value("MIDDLE",         AbilityHand::MIDDLE)
        .value("RING",           AbilityHand::RING)
        .value("PINKY",          AbilityHand::PINKY)
        .value("THUMB_FLEXOR",   AbilityHand::THUMB_FLEXOR)
        .value("THUMB_ROTATOR",  AbilityHand::THUMB_ROTATOR)
        .export_values();

    py::enum_<AbilityHand::UpsamplingChannel>(m, "UpsamplingChannel")
        .value("UPSAMPLING_INDEX",         AbilityHand::UPSAMPLING_INDEX)
        .value("UPSAMPLING_MIDDLE",        AbilityHand::UPSAMPLING_MIDDLE)
        .value("UPSAMPLING_RING",          AbilityHand::UPSAMPLING_RING)
        .value("UPSAMPLING_PINKY",         AbilityHand::UPSAMPLING_PINKY)
        .value("UPSAMPLING_THUMB_FLEXOR",  AbilityHand::UPSAMPLING_THUMB_FLEXOR)
        .value("UPSAMPLING_THUMB_ROTATOR", AbilityHand::UPSAMPLING_THUMB_ROTATOR)
        .value("UPSAMPLING_ALL",           AbilityHand::UPSAMPLING_ALL)
        .export_values();

    // -------------------------------------------------------------------------
    // AbilityHand class
    // -------------------------------------------------------------------------
    py::class_<AbilityHand>(m, "AbilityHand")
        .def(py::init<>())

        // Public member variables
        .def_readwrite("hand_state",    &AbilityHand::hand_state)
        .def_readwrite("stuffing_mode", &AbilityHand::stuffing_mode)

        // Initialization
        .def("init",
            py::overload_cast<unsigned long>(&AbilityHand::init),
            py::arg("baud_rate") = 921600)
        .def("init",
            py::overload_cast<const char*, unsigned long>(&AbilityHand::init),
            py::arg("port_name"), py::arg("baud_rate") = 921600)
        .def("init_udp",
            py::overload_cast<const char*, uint16_t>(&AbilityHand::init_udp),
            py::arg("ip"), py::arg("port"))
        .def("init_udp",
            py::overload_cast<const UdpEndpoint&>(&AbilityHand::init_udp),
            py::arg("endpoint"))
        .def("discover_forwarding_servers", &AbilityHand::discover_forwarding_servers,
            py::arg("timeout"))
        .def("disconnect",    &AbilityHand::disconnect)
        .def("is_connected",  &AbilityHand::is_connected)

        // Mode setting
        .def("set_motion_mode", &AbilityHand::set_motion_mode, py::arg("mode"))
        .def("set_short_mode",  &AbilityHand::set_short_mode,  py::arg("mode"))

        // Register access — out-param reply wrapped into return tuple (int, RegisterReply)
        .def("read_register",
            [](AbilityHand& h, uint32_t addr, int timeout_ms) {
                abh_register_reply_t reply{};
                int ret = h.read_register(addr, reply, timeout_ms);
                return py::make_tuple(ret, reply);
            },
            py::arg("addr"), py::arg("timeout_ms") = 50)
        .def("write_register",
            [](AbilityHand& h, uint32_t addr, uint32_t value, int timeout_ms) {
                abh_register_reply_t reply{};
                int ret = h.write_register(addr, value, reply, timeout_ms);
                return py::make_tuple(ret, reply);
            },
            py::arg("addr"), py::arg("value"), py::arg("timeout_ms") = 50)

        // Upsampling
        .def("enable_upsampling",
            &AbilityHand::enable_upsampling,
            py::arg("mask") = AbilityHand::UPSAMPLING_ALL)
        .def("disable_upsampling",
            &AbilityHand::disable_upsampling,
            py::arg("mask") = AbilityHand::UPSAMPLING_ALL)

        // Core communication
        .def("exchange_motion_data", &AbilityHand::exchange_motion_data,
            py::arg("timeout_ms") = 50)
        .def("exchange_short_msg",   &AbilityHand::exchange_short_msg,
            py::arg("timeout_ms") = 50)

        // command_handling — out-param responses wrapped into return tuple (int, list[str])
        .def("command_handling",
            [](AbilityHand& h, const std::string& payload,
               int nresponses_expected, int timeout_ms) {
                std::vector<std::string> responses;
                int ret = h.command_handling(payload, responses, nresponses_expected, timeout_ms);
                return py::make_tuple(ret, responses);
            },
            py::arg("payload"), py::arg("nresponses_expected"), py::arg("timeout_ms") = 50)

        // Setters — accept Python list, validated and converted to C array
        .def("set_position_targets",
            [](AbilityHand& h, py::list pos) {
                if ((int)pos.size() != NUM_CHANNELS)
                    throw std::invalid_argument("Expected " + std::to_string(NUM_CHANNELS) + " values");
                float arr[NUM_CHANNELS];
                for (int i = 0; i < NUM_CHANNELS; i++) arr[i] = pos[i].cast<float>();
                h.set_position_targets(arr);
            },
            py::arg("positions_deg"))
        .def("set_velocity_targets",
            [](AbilityHand& h, py::list vel) {
                if ((int)vel.size() != NUM_CHANNELS)
                    throw std::invalid_argument("Expected " + std::to_string(NUM_CHANNELS) + " values");
                float arr[NUM_CHANNELS];
                for (int i = 0; i < NUM_CHANNELS; i++) arr[i] = vel[i].cast<float>();
                h.set_velocity_targets(arr);
            },
            py::arg("velocities_deg_per_sec"))
        .def("set_torque_targets",
            [](AbilityHand& h, py::list trq) {
                if ((int)trq.size() != NUM_CHANNELS)
                    throw std::invalid_argument("Expected " + std::to_string(NUM_CHANNELS) + " values");
                float arr[NUM_CHANNELS];
                for (int i = 0; i < NUM_CHANNELS; i++) arr[i] = trq[i].cast<float>();
                h.set_torque_targets(arr);
            },
            py::arg("torques_mNm"))
        .def("set_voltage_targets",
            [](AbilityHand& h, py::list volt) {
                if ((int)volt.size() != NUM_CHANNELS)
                    throw std::invalid_argument("Expected " + std::to_string(NUM_CHANNELS) + " values");
                float arr[NUM_CHANNELS];
                for (int i = 0; i < NUM_CHANNELS; i++) arr[i] = volt[i].cast<float>();
                h.set_voltage_targets(arr);
            },
            py::arg("voltage_percent"))

        // Getters — fill C array and return as Python list
        .def("get_positions",
            [](const AbilityHand& h) {
                float arr[NUM_CHANNELS];
                h.get_positions(arr);
                return std::vector<float>(arr, arr + NUM_CHANNELS);
            })
        .def("get_velocities",
            [](const AbilityHand& h) {
                float arr[NUM_CHANNELS];
                h.get_velocities(arr);
                return std::vector<float>(arr, arr + NUM_CHANNELS);
            })
        .def("get_currents",
            [](const AbilityHand& h) {
                float arr[NUM_CHANNELS];
                h.get_currents(arr);
                return std::vector<float>(arr, arr + NUM_CHANNELS);
            })
        .def("get_fsr_forces",
            [](const AbilityHand& h) {
                float arr[NUM_FINGERS * NUM_FSR_PER_FINGER];
                h.get_fsr_forces(arr);
                return std::vector<float>(arr, arr + NUM_FINGERS * NUM_FSR_PER_FINGER);
            })

        // Statistics
        .def("get_statistics",   &AbilityHand::get_statistics)
        .def("reset_statistics", &AbilityHand::reset_statistics)

        // Advanced — direct hand state reference
        .def("get_hand_state",
            [](AbilityHand& h) -> abh_api_t& { return h.get_hand_state(); },
            py::return_value_policy::reference_internal);
}
