#pragma once
#include <filesystem>

// Recover only this adapter's validated publication journals. Unknown files and
// malformed journals are preserved, and a failure prevents overwriting the slot.
bool CtSwitchFilesRecover(const std::filesystem::path &path);
bool CtSwitchFilesRecoverDirectory(const std::filesystem::path &directory);
