cmake_minimum_required(VERSION 3.25)
project(chronicletwo_runtime_checks LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(CT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(CT_PC_BUILD "${CT_ROOT}/port/build/windows-x64-release" CACHE PATH "Prepared PC tree")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" -c
    "import sys; from pathlib import Path; sys.path.insert(0, '${CT_ROOT}/scripts/switch'); from prepare_runtime import prepare_scheduler; prepare_scheduler(Path('${CMAKE_BINARY_DIR}/generated'))"
    COMMAND_ERROR_IS_FATAL ANY)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
add_subdirectory("${CT_PC_BUILD}/_deps/googletest-src" googletest EXCLUDE_FROM_ALL)
add_executable(runtime_checks main.cpp memory_test.cpp platform_memory_win.c
    "${CT_ROOT}/port/switch/runtime/memory.cpp"
    "${CT_ROOT}/tools/kazarov/src/cpp/platform/arena.cpp"
    "${CT_ROOT}/tools/kazarov/src/cpp/platform/clock.cpp"
    "${CT_ROOT}/tools/kazarov/src/cpp/platform/log.cpp"
    "${CMAKE_BINARY_DIR}/generated/eekernel_threads.cpp"
    "${CT_ROOT}/tools/kazarov/src/cpp/sce/unimplemented.cpp"
    "${CT_ROOT}/tools/kazarov/src/cpp/tests/platform_arena_memory_test.cpp"
    "${CT_ROOT}/tools/kazarov/src/cpp/tests/platform_arena_test.cpp"
    "${CT_ROOT}/port/src/tests/eekernel_threads_test.cpp")
target_compile_definitions(runtime_checks PRIVATE PORT)
target_compile_options(runtime_checks PRIVATE -include "${CT_ROOT}/tools/kazarov/include/kazarov/port.h")
target_include_directories(runtime_checks PRIVATE "${CT_ROOT}/tools/kazarov/include"
    "${CT_ROOT}/port/switch" "${CT_PC_BUILD}/ps2_include/sce")
target_link_libraries(runtime_checks PRIVATE GTest::gtest)
enable_testing()
include(GoogleTest)
gtest_discover_tests(runtime_checks DISCOVERY_TIMEOUT 20 PROPERTIES TIMEOUT 30)
