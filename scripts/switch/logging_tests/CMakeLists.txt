cmake_minimum_required(VERSION 3.25)
project(chronicletwo_logging_checks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(CT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(CT_PC_BUILD "${CT_ROOT}/port/build/windows-x64-release" CACHE PATH "Built PC dependency tree")
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
add_subdirectory("${CT_PC_BUILD}/_deps/googletest-src" googletest EXCLUDE_FROM_ALL)
enable_testing()
include(GoogleTest)
foreach(backend IN ITEMS quill sync)
    set(target log_${backend}_checks)
    add_executable(${target} main.cpp "${CT_ROOT}/port/src/tests/log_test.cpp"
        "${CT_ROOT}/port/src/log/log_common.cpp" "${CT_ROOT}/port/src/log/ps2_printf.cpp"
        "${CT_ROOT}/tools/kazarov/src/cpp/platform/log.cpp"
        "${CT_ROOT}/tools/kazarov/src/cpp/platform/settings.cpp")
    target_include_directories(${target} PRIVATE "${CT_ROOT}/port/src" "${CT_ROOT}/port/include"
        "${CT_ROOT}/tools/kazarov/include" "${CT_ROOT}/port/src/tests")
    target_compile_definitions(${target} PRIVATE CT_LOG_STANDALONE)
    target_link_libraries(${target} PRIVATE GTest::gtest)
    if(backend STREQUAL "sync")
        target_sources(${target} PRIVATE "${CT_ROOT}/port/src/log/log_sync.cpp" sync_test.cpp)
        target_compile_definitions(${target} PRIVATE CT_LOG_SYNC)
    else()
        target_sources(${target} PRIVATE "${CT_ROOT}/port/src/log/log.cpp")
        target_include_directories(${target} PRIVATE "${CT_PC_BUILD}/_deps/quill-src/include")
        target_compile_definitions(${target} PRIVATE QUILL_DISABLE_NON_PREFIXED_MACROS QUILL_DISABLE_FUNCTION_NAME
            QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_TRACE_L1)
    endif()
    # The game logger has process-lifetime state; use the main repository's one-process-per-case policy.
    gtest_discover_tests(${target} TEST_PREFIX "${backend}." DISCOVERY_TIMEOUT 20 PROPERTIES TIMEOUT 45)
endforeach()
add_executable(log_probe_checks probe_main.cpp "${CT_ROOT}/port/switch/core_probe/log_checks.cpp"
    "${CT_ROOT}/port/src/log/log_sync.cpp" "${CT_ROOT}/port/src/log/log_common.cpp"
    "${CT_ROOT}/port/src/log/ps2_printf.cpp" "${CT_ROOT}/tools/kazarov/src/cpp/platform/log.cpp"
    "${CT_ROOT}/tools/kazarov/src/cpp/platform/settings.cpp")
target_include_directories(log_probe_checks PRIVATE "${CT_ROOT}/port/src" "${CT_ROOT}/port/include"
    "${CT_ROOT}/tools/kazarov/include")
target_compile_definitions(log_probe_checks PRIVATE CT_LOG_SYNC)
add_test(NAME log_probe_checks COMMAND log_probe_checks "${CMAKE_CURRENT_BINARY_DIR}/probe/saves")
set_tests_properties(log_probe_checks PROPERTIES TIMEOUT 45)
