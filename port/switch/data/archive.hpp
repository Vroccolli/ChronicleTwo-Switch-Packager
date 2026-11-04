#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Native Switch implementation of the read-only asset archive interface.
// The PC keeps its Rust implementation, including its extraction/packing tools.
namespace ct::archive {
template<class T> using Box = std::unique_ptr<T>;
template<class T> using Slice = std::span<T>;
using Str = std::string_view;
struct Entry {
    std::string name;
    uint64_t size=0, compressed=0, offset=0;
    uint32_t crc=0;
    uint16_t method=0;
};
class Archive {
public:
    explicit Archive(const std::filesystem::path &path);
    std::vector<uint8_t> Bytes(uint64_t offset,uint64_t size) const;
    std::vector<Entry> entries;
private:
    uint64_t length=0;
    mutable std::mutex mutex;
    mutable std::ifstream file;
};
Box<Archive> open_archive(Str path);
size_t archive_entry_count(const Archive &archive);
Str archive_entry_name(const Archive &archive,size_t index);
uint64_t archive_entry_size(const Archive &archive,size_t index);
uint32_t archive_entry_crc(const Archive &archive,size_t index);
bool archive_entry_stored(const Archive &archive,size_t index);
uint64_t archive_entry_data_offset(const Archive &archive,size_t index);
void archive_read_entry(const Archive &archive,size_t index,Slice<uint8_t> out);
}
