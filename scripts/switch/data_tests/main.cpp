#include "data/archive.hpp"
#include <zstd.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
int main(int argc,char **argv) {
    try {
        if (argc==4 && std::string(argv[1])=="--zstd") {
            std::ifstream input(argv[2],std::ios::binary);
            std::string data((std::istreambuf_iterator<char>(input)),{});
            std::vector<uint8_t> compressed(ZSTD_compressBound(data.size()));
            auto size=ZSTD_compress(compressed.data(),compressed.size(),data.data(),data.size(),3);
            if (ZSTD_isError(size)) return 2;
            std::ofstream output(argv[3],std::ios::binary);
            output.write(reinterpret_cast<const char *>(compressed.data()),size);
            return output?0:3;
        }
        if (argc!=2) return 2;
        auto archive=ct::archive::open_archive(argv[1]);
        std::printf("count %zu\n",archive->entries.size());
        for (size_t i=0;i<archive->entries.size();++i) {
            const auto &entry=archive->entries[i];
            // The prepared game includes movies up to 179 MB. Keep a bounded
            // host allocation while exercising those real archive entries too.
            if (entry.size>256*1024*1024) throw std::runtime_error("host test allocation limit");
            std::vector<uint8_t> bytes(entry.size);
            ct::archive::archive_read_entry(*archive,i,bytes);
            std::printf("%s\t%llu\t%08x\n",entry.name.c_str(),static_cast<unsigned long long>(entry.size),entry.crc);
        }
        return 0;
    } catch (const std::exception &error) { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
