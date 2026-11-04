#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace ct::data {
inline std::optional<uint32_t> NextUtf8(std::string_view text,size_t &pos) {
    if (pos==text.size()) return {};
    uint32_t cp=uint8_t(text[pos++]),minimum=0; size_t remaining=0;
    if (cp>=0xc2 && cp<=0xdf) { cp&=31; remaining=1; minimum=0x80; }
    else if (cp>=0xe0 && cp<=0xef) { cp&=15; remaining=2; minimum=0x800; }
    else if (cp>=0xf0 && cp<=0xf4) { cp&=7; remaining=3; minimum=0x10000; }
    else if (cp>=0x80) return {};
    while (remaining) {
        --remaining;
        if (pos==text.size() || (uint8_t(text[pos])&0xc0)!=0x80) return {};
        cp=(cp<<6)|(uint8_t(text[pos++])&63);
    }
    if (cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return {};
    return cp;
}
inline void AppendUtf8(std::string &text,uint32_t cp) {
    if (cp<0x80) text+=static_cast<char>(cp);
    else if (cp<0x800) { text+=static_cast<char>(0xc0|(cp>>6)); text+=static_cast<char>(0x80|(cp&63)); }
    else if (cp<0x10000) { text+=static_cast<char>(0xe0|(cp>>12)); text+=static_cast<char>(0x80|((cp>>6)&63)); text+=static_cast<char>(0x80|(cp&63)); }
    else { text+=static_cast<char>(0xf0|(cp>>18)); text+=static_cast<char>(0x80|((cp>>12)&63)); text+=static_cast<char>(0x80|((cp>>6)&63)); text+=static_cast<char>(0x80|(cp&63)); }
}
}
