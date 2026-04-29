# Hand Serial Test

## Introduction

This codebase is a mixed C/C++ API wrapper for the Ability Hand Extended Communication Interface. It can be used as a statically linkable API for C++ native real-time control of an Ability Hand over serial. It includes hardware communication, using both serial ports directly and optionally interfacing with serial port forwarding servers.

## Initial Setup

This project uses git submodules for dependencies. When cloning, use one of the following methods:

1. Clone with submodules (recommended)
```bash
git clone --recurse-submodules https://github.com/psyonicinc/hand-serial-test.git
```

2. If you already cloned without submodules
If you've already cloned the repository without the `--recurse-submodules` flag, you can initialize and fetch the submodules with:

```bash
git submodule update --init --recursive
```

This command will:
- `--init`: Initialize your local submodule configuration
- `--recursive`: Fetch all nested submodules (if any exist)

## Building

### Library only (default)

```bash
cmake -S . -B build
cmake --build build
```

This produces `ability_hand.lib` (Windows) or `libability_hand.a` (Linux/macOS) with no executables.

### With example applications

```bash
cmake -S . -B build -DBUILD_EXAMPLES=ON
cmake --build build
```

Builds `example_udp` (UDP control loop) and `example_serial` (serial control loop) in addition to the library.

### With Python bindings

Requires Python 3 and a CPython interpreter accessible to CMake.

```bash
cmake -S . -B build -DBUILD_PYTHON_BINDINGS=ON
cmake --build build --config Release
```

Produces `abh.cpython-<ver>-<platform>.pyd` (Windows) or `abh.cpython-<ver>-<platform>.so` (Linux/macOS) under `build/python/Release/`. See [doc/python_api.md](doc/python_api.md) for usage.

---

## Using as a Dependency

### Option 1 — FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    ability_hand
    GIT_REPOSITORY https://github.com/psyonicinc/hand-serial-test.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(ability_hand)

target_link_libraries(your_target PRIVATE ability_hand)
```

### Option 2 — git submodule

```bash
git submodule add https://github.com/psyonicinc/hand-serial-test.git external/ability_hand
git submodule update --init --recursive
```

```cmake
add_subdirectory(external/ability_hand)
target_link_libraries(your_target PRIVATE ability_hand)
```

In both cases, include directories and transitive dependencies (`serial`, `PPP`, `cobs`, and Windows socket libs) propagate automatically. If your project already defines any of those targets, `ability_hand` will detect them and skip re-adding the subdirectories.

## Testing with Ceedling

This project uses Ceedling for unit testing.

### Prerequisites
- Ruby
- Ceedling gem: `gem install ceedling`

### Running Tests

Navigate to the tests directory and run:
```bash
cd tests
ceedling test:all
```
