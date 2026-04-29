pybind11_add_module(ability_hand_py py_bindings.cpp)

target_link_libraries(ability_hand_py PRIVATE ability_hand)

set_target_properties(ability_hand_py PROPERTIES
    OUTPUT_NAME abh
)
