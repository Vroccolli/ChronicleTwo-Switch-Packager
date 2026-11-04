#include "archive.hpp"
#include <zstd.h>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace ct::archive {
namespace {
[[noreturn]] void Fail(const char *message) { throw std::runtime_error(message); }
uint64_t Add(uint64_t a,uint64_t b) {
    if (b>std::numeric_limits<uint64_t>::max()-a) Fail("archive offset overflow");
    return a+b;
}
uint64_t Word(std::span<const uint8_t> bytes,size_t at,size_t count) {
    if (at>bytes.size() || count>bytes.size()-at) Fail("truncated archive field");
    uint64_t value=0;
    for (size_t i=0;i<count;++i) value|=uint64_t(bytes[at+i])<<(i*8);
    return value;
}
std::string Fold(std::string name) {
    for (char &c:name) if (c>='A' && c<='Z') c+='a'-'A';
    return name;
}
void ValidateName(std::string_view name) {
    if (name.empty() || name.size()>65535) Fail("invalid archive entry name");
    for (size_t pos=0;pos<name.size();) {
        uint32_t cp=uint8_t(name[pos++]), minimum=0; size_t remaining=0;
        if (cp>=0xc2 && cp<=0xdf) { cp&=31; remaining=1; minimum=0x80; }
        else if (cp>=0xe0 && cp<=0xef) { cp&=15; remaining=2; minimum=0x800; }
        else if (cp>=0xf0 && cp<=0xf4) { cp&=7; remaining=3; minimum=0x10000; }
        else if (cp>=0x80) Fail("archive entry name is not UTF-8");
        while (remaining) {
            --remaining;
            if (pos==name.size() || (uint8_t(name[pos])&0xc0)!=0x80) Fail("archive entry name is not UTF-8");
            cp=(cp<<6)|(uint8_t(name[pos++])&63);
        }
        if (cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) Fail("invalid UTF-8 scalar in archive name");
        if (cp<32 || (cp>=127 && cp<=159) || cp=='\\' || cp==':') Fail("invalid archive entry name");
    }
    size_t at=0;
    while (at<=name.size()) {
        auto end=name.find('/',at); if (end==name.npos) end=name.size();
        auto part=name.substr(at,end-at);
        if (part.empty() || part=="." || part=="..") Fail("invalid archive path component");
        if (end==name.size()) break;
        at=end+1;
    }
}
constexpr auto CrcTable() {
    std::array<uint32_t,256> table{};
    for (uint32_t i=0;i<256;++i) { auto value=i; for (int j=0;j<8;++j) value=(value>>1)^((value&1)?0xedb88320u:0); table[i]=value; }
    return table;
}
uint32_t Crc(std::span<const uint8_t> bytes) {
    static constexpr auto table=CrcTable(); uint32_t crc=0xffffffff;
    for (auto value:bytes) crc=(crc>>8)^table[(crc^value)&255];
    return ~crc;
}
}
std::vector<uint8_t> Archive::Bytes(uint64_t offset,uint64_t size) const {
    if (offset>length || size>length-offset || size>std::numeric_limits<size_t>::max() ||
        size>uint64_t(std::numeric_limits<std::streamsize>::max()) || offset>uint64_t(std::numeric_limits<std::streamoff>::max()))
        Fail("archive read outside file");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    const std::lock_guard lock(mutex);
    file.clear(); file.seekg(static_cast<std::streamoff>(offset));
    if (size) file.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(size));
    if (!file) Fail("cannot read archive bytes");
    return bytes;
}
Archive::Archive(const std::filesystem::path &path):file(path,std::ios::binary) {
    if (!file) Fail("cannot open asset archive");
    file.seekg(0,std::ios::end); auto end=file.tellg(); if (end<0) Fail("cannot size asset archive");
    length=static_cast<uint64_t>(end);
    if (length<22) Fail("not a ZIP archive");
    auto tailSize=std::min<uint64_t>(length,22+65535); auto tailOffset=length-tailSize;
    auto tail=Bytes(tailOffset,tailSize); size_t endAt=tail.size();
    for (size_t at=tail.size()-22;;--at) {
        if (Word(tail,at,4)==0x06054b50 && at+22+Word(tail,at+20,2)==tail.size()) { endAt=at; break; }
        if (!at) break;
    }
    if (endAt==tail.size()) Fail("ZIP end record missing");
    uint64_t disk=Word(tail,endAt+4,2),directoryDisk=Word(tail,endAt+6,2),diskCount=Word(tail,endAt+8,2);
    uint64_t count=Word(tail,endAt+10,2),directorySize=Word(tail,endAt+12,4),directoryOffset=Word(tail,endAt+16,4);
    uint64_t limit=tailOffset+endAt;
    bool zip64=false;
    if (limit>=20) {
        auto locator=Bytes(limit-20,20);
        if (Word(locator,0,4)==0x07064b50) {
            zip64=true;
            if (Word(locator,4,4)!=0 || Word(locator,16,4)!=1) Fail("multi-disk archive unsupported");
            auto offset=Word(locator,8,8);
            if (Add(offset,56)>limit-20) Fail("ZIP64 end record outside file");
            auto record=Bytes(offset,56);
            if (Word(record,0,4)!=0x06064b50 || Word(record,4,8)<44) Fail("invalid ZIP64 end record");
            disk=Word(record,16,4); directoryDisk=Word(record,20,4); diskCount=Word(record,24,8); count=Word(record,32,8);
            directorySize=Word(record,40,8); directoryOffset=Word(record,48,8); limit=offset;
        }
    }
    if (!zip64 && (count==65535 || directorySize==0xffffffff || directoryOffset==0xffffffff)) Fail("ZIP64 locator missing");
    if (disk || directoryDisk || diskCount!=count) Fail("multi-disk archive unsupported");
    if (Add(directoryOffset,directorySize)>limit || count>directorySize/46) Fail("invalid central directory bounds");
    auto directory=Bytes(directoryOffset,directorySize);
    entries.reserve(static_cast<size_t>(count)); std::unordered_set<std::string> names;
    size_t at=0;
    for (uint64_t i=0;i<count;++i) {
        if (Word(directory,at,4)!=0x02014b50) Fail("invalid central directory entry");
        auto fields=std::span<const uint8_t>(directory).subspan(at);
        if (fields.size()<46) Fail("truncated central header");
        Entry entry; auto flags=Word(fields,8,2); entry.method=Word(fields,10,2); entry.crc=Word(fields,16,4);
        entry.compressed=Word(fields,20,4); entry.size=Word(fields,24,4); auto local=Word(fields,42,4);
        auto nameSize=Word(fields,28,2),extraSize=Word(fields,30,2),commentSize=Word(fields,32,2),startDisk=Word(fields,34,2);
        auto next=Add(46,Add(nameSize,Add(extraSize,commentSize)));
        if (next>fields.size()) Fail("truncated central directory entry");
        entry.name.assign(reinterpret_cast<const char *>(fields.data()+46),nameSize); ValidateName(entry.name);
        if (!names.insert(Fold(entry.name)).second) Fail("duplicate archive path");
        if (flags&(1|64|8192)) Fail("encrypted asset archive unsupported");
        if (entry.method!=0 && entry.method!=93) Fail("unsupported asset compression method");
        auto extra=fields.subspan(46+nameSize,extraSize);
        bool needSize=entry.size==0xffffffff,needCompressed=entry.compressed==0xffffffff;
        bool needLocal=local==0xffffffff,needDisk=startDisk==65535,sawZip64=false;
        while (extra.size()>=4) {
            auto id=Word(extra,0,2),size=Word(extra,2,2);
            if (size>extra.size()-4) Fail("truncated extra field");
            if (id==1) {
                if (sawZip64) Fail("duplicate ZIP64 extra field");
                sawZip64=true;
                auto data=extra.subspan(4,size); size_t cursor=0;
                auto take=[&](uint64_t &value,bool &needed) { if (needed) { value=Word(data,cursor,8); cursor+=8; needed=false; } };
                take(entry.size,needSize); take(entry.compressed,needCompressed); take(local,needLocal);
                if (needDisk) { startDisk=Word(data,cursor,4); needDisk=false; }
            }
            extra=extra.subspan(4+size);
        }
        if (startDisk || needSize || needCompressed || needLocal || needDisk) Fail("missing/invalid ZIP64 fields");
        if (entry.method==0 && entry.compressed!=entry.size) Fail("stored entry size mismatch");
        auto header=Bytes(local,30);
        if (Word(header,0,4)!=0x04034b50 || Word(header,8,2)!=entry.method || (Word(header,6,2)&(1|64|8192))) Fail("invalid local header");
        auto localNameSize=Word(header,26,2),localExtraSize=Word(header,28,2);
        auto localName=Bytes(Add(local,30),localNameSize);
        if (localName.size()!=entry.name.size() || !std::equal(localName.begin(),localName.end(),reinterpret_cast<const uint8_t *>(entry.name.data()))) Fail("local entry name mismatch");
        entry.offset=Add(local,Add(30,Add(localNameSize,localExtraSize)));
        if (Add(entry.offset,entry.compressed)>directoryOffset) Fail("entry data overlaps central directory");
        entries.push_back(std::move(entry)); at+=static_cast<size_t>(next);
    }
}
Box<Archive> open_archive(Str path) { return std::make_unique<Archive>(std::filesystem::path(path)); }
size_t archive_entry_count(const Archive &a) { return a.entries.size(); }
Str archive_entry_name(const Archive &a,size_t i) { return a.entries.at(i).name; }
uint64_t archive_entry_size(const Archive &a,size_t i) { return a.entries.at(i).size; }
uint32_t archive_entry_crc(const Archive &a,size_t i) { return a.entries.at(i).crc; }
bool archive_entry_stored(const Archive &a,size_t i) { return a.entries.at(i).method==0; }
uint64_t archive_entry_data_offset(const Archive &a,size_t i) { return a.entries.at(i).offset; }
void archive_read_entry(const Archive &a,size_t i,Slice<uint8_t> out) {
    const auto &entry=a.entries.at(i);
    if (entry.size!=out.size()) Fail("archive destination size mismatch");
    auto bytes=a.Bytes(entry.offset,entry.compressed);
    if (entry.method==0) std::copy(bytes.begin(),bytes.end(),out.begin());
    else {
        auto size=ZSTD_decompress(out.data(),out.size(),bytes.data(),bytes.size());
        if (ZSTD_isError(size) || size!=out.size()) Fail("invalid Zstandard asset payload");
    }
    if (Crc(out)!=entry.crc) Fail("asset CRC mismatch");
}
}
