# DHL Build and Testing Guide

## Quick Start

### Linux/Unix (Makefile)
```bash
cd /path/to/DHL
make clean && make all
./test_dhl
./test_qc_dhl
```

### Linux/Unix (CMake)
```bash
cd /path/to/DHL
mkdir cmake_build && cd cmake_build
cmake .. && cmake --build .
cd .. && ./cmake_build/test_dhl
```

## Detailed Build Instructions

## Linux/Unix Testing (Make)

### Build All Targets
```bash
make all
```

### Build Individual Targets
```bash
make index       # Build index executable
make query       # Build query executable  
make update      # Build update executable
make test_dhl    # Build DHL test program
make test_qc     # Build Quezon City test program
```

### Clean Build
```bash
make clean
make all
```

### Single Command Test
```bash
make test_qc_dhl && ./test_qc_dhl
make test_dhl && ./test_dhl
```

### Prerequisites
- CMake (version 3.10 or higher)
- A C++ compiler that supports C++20 (Visual Studio 2019+, GCC 10+, or Clang 10+)
- Git (optional, for cloning)


### Build Steps

#### Step 1: Create Build Directory
```bash
mkdir -p build 
cd build
```

#### Step 2: Configure Build
```bash
# Linux/macOS with default generator
cmake ..

# Windows with Visual Studio (Optional)
cmake .. -sG "Visual Studio 16 2019"

# Windows with MinGW (Optional)
cmake .. -G "MinGW Makefiles"

# Cross-platform with Ninja (optional if installed)
cmake .. -G "Ninja"
```

#### Step 3: Build Project
```bash
cmake --build .

# Or for specific configuration (Windows)
cmake --build . --config Release
```

#### Step 4: Run Tests
```bash
# From DHL root directory (recommended - data files are here)
cd ..
./build/test_dhl
./build/test_qc_dhl

# Or copy data files to build directory
# cd cmake_build
# cp ../*.txt .
# ./test_dhl
```







