# Hand Serial Test

## Introduction

This codebase is dedicated for functional testing of Ability Hands, using the various UART command headers. 

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

### Build Instructions

1. Ensure submodules are initialized (see "Cloning the Repository" above)

2. Create a build directory and run CMake:
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

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
