set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Create a library for the static HC2L core
add_library(hc2l_static_lib
    src/road_network.cpp
    src/util.cpp
)

target_include_directories(hc2l_static_lib PUBLIC src)

# CLI for building index
add_executable(hc2l_cli_build src/cli_build.cpp)
target_link_libraries(hc2l_cli_build PRIVATE hc2l_static_lib)

# CLI for OD query evaluation
add_executable(hc2l_query_od src/cli_query_od.cpp)
target_link_libraries(hc2l_query_od PRIVATE hc2l_static_lib)
