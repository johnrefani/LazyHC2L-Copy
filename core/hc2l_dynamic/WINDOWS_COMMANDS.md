# HC2L Dynamic Testing Commands (Windows)

## Quick Setup

### Option 1: Using Ninja (Recommended)
```cmd
cd /path/to/directory/LazyHC2L/core/hc2l_dynamic
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_C99=1 -D_GLIBCXX_USE_C99_DYNAMIC=1"
cmake --build . -j 4
```

### Option 2: Using MSYS2 (Alternative)
```cmd
cd /path/to/directory/LazyHC2L/core/hc2l_dynamic
mkdir build
cd build
cmake .. -G "MSYS Makefiles"
cmake --build . -j 4
```

### Option 3: MinGW with Compatibility Fix
```cmd
cd /path/to/directory/LazyHC2L/core/hc2l_dynamic
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_C99=1"
cmake --build . -j 4
```

## Copy Test Data 
not required if there's already dataset on ./tests/test_data.
```cmd
copy ..\..\..\data\processed\qc_from_csv.gr tests\test_data\
copy ..\..\..\data\processed\qc_scenario_for_cpp_1.csv tests\test_data\
```

## Run Tests

### 1. Dedicated QC Scenario Test (Recommended)
```cmd
cd build
cmake --build . --target test_qc_scenario && test_qc_scenario.exe
```

in some case: ``` test_qc_scenario.exe ``` command is not working. 


### 2. Interactive Demo
After running the first command, you should be in the ``` build ``` directory.
```cmd
cmake --build . --target demo_qc_test && demo_qc_test.exe
```

### 3. Unit/Integration Tests
```cmd
cmake --build . --target hc2l_dynamic_tests && hc2l_dynamic_tests.exe
```

### 4. Main Program with QC Data
```cmd
cmake --build . --target main_dynamic && main_dynamic.exe ..\tests\test_data\qc_from_csv.gr ..\tests\test_data\qc_scenario_for_cpp_1.csv
```

### 5. Performance Analysis
```cmd
cmake --build . --target performance_analysis && performance_analysis.exe
```

## Troubleshooting

### Error: 'timespec_get' has not been declared
This is a common MinGW compatibility issue. Try these solutions:

#### Solution A: Install and Use Ninja
```cmd
# Install ninja via chocolatey
choco install ninja

# Or download from: https://github.com/ninja-build/ninja/releases
# Then use Option 1 above
```

#### Solution B: Update MinGW
```cmd
# Update to latest MinGW-w64 version
# Download from: https://www.mingw-w64.org/downloads/
```

#### Solution C: Use Visual Studio Build Tools
```cmd
# Install Visual Studio Build Tools, then:
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release -j 4
```

#### Solution D: Set Compiler Flags in CMakeLists.txt
Add this to your CMakeLists.txt:
```cmake
if(MINGW)
    add_compile_definitions(_GLIBCXX_USE_C99=1)
    add_compile_definitions(_GLIBCXX_USE_C99_DYNAMIC=1)
endif()
```


