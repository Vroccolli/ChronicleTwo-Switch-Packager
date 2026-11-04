cmake_minimum_required(VERSION 3.22)
project(chronicletwo NONE)

# PLATFORM picks what is built: PS2 builds and verifies the retail executable
# (ps2/CMakeLists.txt), PC builds the native port, SWITCH orchestrates ARM64
# diagnostics and the native game build through the installed SDK.
set(PLATFORM PS2 CACHE STRING "Platform to build: PS2, PC or SWITCH")
set_property(CACHE PLATFORM PROPERTY STRINGS PS2 PC SWITCH)
if(PLATFORM STREQUAL "PS2")
    add_subdirectory(ps2)
elseif(PLATFORM STREQUAL "PC")
    # Here rather than in port so ctest finds the tests from the top of the build tree.
    enable_testing()
    add_subdirectory(port)
elseif(PLATFORM STREQUAL "SWITCH")
    add_subdirectory(port/switch)
else()
    message(FATAL_ERROR "PLATFORM must be PS2, PC or SWITCH, not ${PLATFORM}")
endif()
