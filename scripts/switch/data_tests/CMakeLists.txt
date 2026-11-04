cmake_minimum_required(VERSION 3.25)
project(switch_archive_checks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 26)
get_filename_component(CT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(archive_check main.cpp "${CT_ROOT}/port/switch/data/archive.cpp")
target_include_directories(archive_check PRIVATE "${CT_ROOT}/port/switch" "C:/devkitPro/portlibs/switch/include")
# This is a native host test. Reuse only the Windows archive's C zstd codec
# objects, never a Switch library; no game/GUI/tooling functions are referenced.
set(CT_HOST_ZSTD_ARCHIVE "${CT_ROOT}/port/build/windows-x64-release/cargo/kazarov_54872/x86_64-pc-windows-gnullvm/release/libkazarov_core.a" CACHE FILEPATH "Native host zstd implementation")
target_link_libraries(archive_check PRIVATE "${CT_HOST_ZSTD_ARCHIVE}")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
enable_testing()
add_test(NAME SwitchArchive.CompatibilityAndCorruption COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/verify.py" "$<TARGET_FILE:archive_check>" "${CMAKE_BINARY_DIR}/fixtures")
get_filename_component(CT_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
set_tests_properties(SwitchArchive.CompatibilityAndCorruption PROPERTIES TIMEOUT 30
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CT_COMPILER_BIN}")
add_executable(encoding_check encoding_main.cpp "${CT_ROOT}/port/switch/data/encoding.cpp")
target_include_directories(encoding_check PRIVATE "${CT_ROOT}/port/src")
add_test(NAME SwitchEncoding.WindowsCp932Parity COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/encoding_verify.py" "$<TARGET_FILE:encoding_check>")
set_tests_properties(SwitchEncoding.WindowsCp932Parity PROPERTIES TIMEOUT 30
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CT_COMPILER_BIN}")
