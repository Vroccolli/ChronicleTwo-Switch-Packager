#include "crash/crash_detail.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

namespace crash_detail {
std::string_view CrashKindName(CrashKind kind) {
    switch (kind) {
        case CrashKind::Assert:
            return "assert";
        case CrashKind::Unimplemented:
            return "unimplemented";
        case CrashKind::Fatal:
            return "fatal";
        case CrashKind::Abort:
            return "abort";
        case CrashKind::Terminate:
            return "terminate";
        case CrashKind::Signal:
            return "crash";
    }
    return "?";
}

void FormatHeader(Text &out, const HeaderInfo &info) {
    out.Put("Dark Chronicle crash report\n");
    out.Put("kind:       ").Put(info.kind).Put('\n');
    out.Put("session:    ").Put(info.session.empty() ? "-" : info.session).Put('\n');
    out.Put("build id:   ").Put(info.build_id.empty() ? "-" : info.build_id).Put('\n');
    out.Put("build type: ").Put(info.build_type.empty() ? "-" : info.build_type).Put('\n');
    out.Put("platform:   ").Put(info.platform).Put('\n');
    out.Put("time:       ").Put(info.time_utc).Put('\n');
    out.Put("frame:      ");
    if (info.frame >= 0) {
        out.Dec(info.frame);
    } else {
        out.Put('-');
    }
    out.Put('\n');
}

std::string RedactPath(std::string_view text, const RedactRoots &roots) {
    struct Replacement {
        std::string      from;
        std::string_view to;
    };

    std::vector<Replacement> replacements;
    auto                     add = [&](const std::string &root, std::string_view placeholder) {
        std::string trimmed = root;
        while (trimmed.size() > 1 && (trimmed.back() == '/' || trimmed.back() == '\\')) {
            trimmed.pop_back();
        }
        if (trimmed.empty()) {
            return;
        }
        std::string forward = trimmed;
        std::ranges::replace(forward, '\\', '/');
        std::string back = trimmed;
        std::ranges::replace(back, '/', '\\');
        std::string escaped;
        for (const char c : back) {
            escaped += c;
            if (c == '\\') {
                escaped += '\\';
            }
        }
        std::array<std::string, 3> spellings{std::move(escaped), std::move(back), std::move(forward)};
        for (std::string &spelling : spellings) {
            if (std::ranges::none_of(replacements, [&](const Replacement &r) { return r.from == spelling; })) {
                replacements.push_back({.from = std::move(spelling), .to = placeholder});
            }
        }
    };
    add(roots.save_root, "<save>");
    add(roots.data_root, "<data>");
    add(roots.home, "<home>");
    std::ranges::stable_sort(replacements, [](const Replacement &a, const Replacement &b) { return a.from.size() > b.from.size(); });

#ifdef _WIN32
    constexpr bool fold_case = true;
#else
    constexpr bool fold_case = false;
#endif
    auto matches = [&](std::string_view at, std::string_view root) {
        if (at.size() < root.size()) {
            return false;
        }
        for (std::size_t i = 0; i < root.size(); i++) {
            const char a = at[i];
            const char b = root[i];
            if (a != b && (!fold_case || std::tolower(static_cast<unsigned char>(a)) != std::tolower(static_cast<unsigned char>(b)))) {
                return false;
            }
        }
        // A root matches whole path components only.
        if (at.size() == root.size()) {
            return true;
        }
        const char next = at[root.size()];
        return next == '/' || next == '\\' || next == '"' || !std::isalnum(static_cast<unsigned char>(next));
    };
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        bool replaced = false;
        for (const Replacement &replacement : replacements) {
            if (matches(text.substr(i), replacement.from)) {
                out += replacement.to;
                i += replacement.from.size();
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            out += text[i++];
        }
    }
    return out;
}

} // namespace crash_detail
