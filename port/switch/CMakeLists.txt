# Host-side orchestration: build_core_probe.py invokes the actual ARM64 SDK tools.
# This target deliberately does not claim to build the complete game.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CT_SWITCH_PC_BUILD "${CMAKE_SOURCE_DIR}/port/build/windows-x64-release" CACHE PATH
    "Configured/built PC tree providing generated sources and compile_commands.json")
set(CT_SWITCH_DEVKITPRO "C:/devkitPro" CACHE PATH "devkitPro SDK root")
add_custom_target(switch_core_probe
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/switch/build_core_probe.py"
        --pc-build "${CT_SWITCH_PC_BUILD}" --devkitpro "${CT_SWITCH_DEVKITPRO}"
        --output "${CMAKE_BINARY_DIR}/core-probe"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building ARM64 core math diagnostic (full engine not yet connected)")
add_custom_target(switch_logging_probe
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/switch/build_core_probe.py"
        --logging --pc-build "${CT_SWITCH_PC_BUILD}" --devkitpro "${CT_SWITCH_DEVKITPRO}"
        --output "${CMAKE_BINARY_DIR}/logging-probe"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building Switch logging diagnostic with real backend and SD file readback")
add_custom_target(switch_runtime_probe
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/switch/build_core_probe.py"
        --runtime --pc-build "${CT_SWITCH_PC_BUILD}" --devkitpro "${CT_SWITCH_DEVKITPRO}"
        --output "${CMAKE_BINARY_DIR}/runtime-probe"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building Switch memory and cooperative-context diagnostic")
add_custom_target(switch_services_probe
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/switch/build_core_probe.py"
        --services --pc-build "${CT_SWITCH_PC_BUILD}" --devkitpro "${CT_SWITCH_DEVKITPRO}"
        --output "${CMAKE_BINARY_DIR}/services-probe"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building controlled-crash services diagnostic")
add_custom_target(switch_engine_link_check
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/switch/link_engine.py"
        --pc-build "${CT_SWITCH_PC_BUILD}" --devkitpro "${CT_SWITCH_DEVKITPRO}"
        --output "${CMAKE_BINARY_DIR}/engine-link" --jobs 4
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Compiling engine ARM64 objects and reporting actual link dependencies")
