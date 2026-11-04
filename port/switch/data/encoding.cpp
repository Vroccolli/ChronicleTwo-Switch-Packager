#include "platform/legacy_encoding.hpp"
#include "utf8.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace {
#include "cp932_table.inc"
template<size_t N> std::optional<uint16_t> Find(const Cp932Pair (&table)[N],uint32_t key) {
    auto entry=std::lower_bound(std::begin(table),std::end(table),key,[](auto pair,auto value) { return pair.key<value; });
    if (entry==std::end(table) || entry->key!=key) return {};
    return entry->value;
}
}
namespace legacy_encoding {
std::string GameText(std::string_view utf8) {
    if (utf8.contains('\0')) throw std::runtime_error("config string contains a null byte");
    std::string result; result.reserve(utf8.size()); size_t pos=0;
    while (pos<utf8.size()) {
        auto scalar=ct::data::NextUtf8(utf8,pos);
        if (!scalar) throw std::runtime_error("config string is not valid UTF-8");
        auto encoded=Find(EncodeMap,*scalar);
        if (!encoded) throw std::runtime_error("config string cannot be represented in CP932");
        if (*encoded>255) result+=static_cast<char>(*encoded>>8);
        result+=static_cast<char>(*encoded&255);
    }
    return result;
}
std::optional<std::string> ReadableText(std::string_view cp932) {
    if (cp932.contains('\0')) return {};
    std::string result; result.reserve(cp932.size());
    for (size_t pos=0;pos<cp932.size();) {
        uint16_t code=uint8_t(cp932[pos++]);
        if ((code>=0x81 && code<=0x9f) || (code>=0xe0 && code<=0xfc)) {
            if (pos==cp932.size()) return {};
            code=(code<<8)|uint8_t(cp932[pos++]);
        }
        auto scalar=Find(DecodeMap,code);
        if (!scalar) return {};
        ct::data::AppendUtf8(result,*scalar);
    }
    return result;
}
}
