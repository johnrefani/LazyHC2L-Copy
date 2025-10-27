#!/bin/bash
set -e

# Configure the CMake build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build all targets (hc2l_cli_build, hc2l_query_od, etc.)
cmake --build build -j
