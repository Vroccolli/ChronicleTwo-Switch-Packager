#include "crash/crash_capture.hpp"
#include "crash/platform.h"
#include <format>
// Preserve ASLR-independent addresses for the exact build's addr2line tool.
// Runtime names/DWARF are deliberately not claimed without an on-device symbol table.
std::string crash_detail::SymbolTableName(std::uint64_t address) {
    const auto base = ctCrashModuleBase();
    return address >= base && address < ctCrashCodeEnd()
        ? std::format("module+0x{:x}", address - base) : "outside module code";
}
