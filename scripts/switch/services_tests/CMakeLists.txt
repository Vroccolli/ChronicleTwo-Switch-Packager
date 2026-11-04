cmake_minimum_required(VERSION 3.25)
project(switch_services_checks LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 26)
get_filename_component(CT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(crash_driver driver.cpp platform_win.c
    "${CT_ROOT}/port/switch/crash/crash.cpp"
    "${CT_ROOT}/port/switch/crash/report.cpp"
    "${CT_ROOT}/port/switch/crash/symbols.cpp"
    "${CT_ROOT}/port/src/crash/crash_context.cpp"
    "${CT_ROOT}/port/src/crash/crash_common.cpp"
    "${CT_ROOT}/port/src/log/log_sync.cpp"
    "${CT_ROOT}/port/src/log/log_common.cpp"
    "${CT_ROOT}/port/src/log/ps2_printf.cpp"
    "${CT_ROOT}/tools/kazarov/src/cpp/platform/log.cpp"
    "${CT_ROOT}/tools/kazarov/src/cpp/platform/settings.cpp")
target_compile_definitions(crash_driver PRIVATE CT_LOG_SYNC)
target_include_directories(crash_driver PRIVATE "${CT_ROOT}/port/src" "${CT_ROOT}/port/switch"
    "${CT_ROOT}/port/include" "${CT_ROOT}/tools/kazarov/include")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
enable_testing()
add_test(NAME Services.ControlledFailures COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/verify.py" "$<TARGET_FILE:crash_driver>" "${CMAKE_BINARY_DIR}/reports")
set_tests_properties(Services.ControlledFailures PROPERTIES TIMEOUT 30)
get_filename_component(CT_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
set_tests_properties(Services.ControlledFailures PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CT_COMPILER_BIN}")
