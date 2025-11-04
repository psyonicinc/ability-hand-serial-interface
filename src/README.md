# Ability Hand Functional Test Suite
## Introduction
This codebase uses open source libraries (hdlc byte stuffing, serial port) to communicate with the Ability Hand. It covers standard API motion control as well as register access commands. The primary use case is functional testing, but this codebase can be easily modified to be pulled in as a submodule or dependency for the serial wrapper. It depends on dartt as well for any future dartt integrations with the Ability Hand.

## Building

To build:
```
mkdir build
cd build
cmake ..
cmake --build .
```

## Testing

To run unit tests:
```
cd tests
ceedling test:all
```
