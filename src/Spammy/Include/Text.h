#pragma once
#include "Headers.h"

struct CaseInsensitiveLess {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const { return una::caseless::compare_utf8(a, b) < 0; }
};

constexpr bool IsUtf8ContinuationByte(char c)
{
    return ((unsigned char)c & 0xC0) == 0x80;
}

template <class Char>
constexpr std::basic_string_view<Char> TrimSpaces(std::basic_string_view<Char> text)
{
    constexpr Char kSpaces[] = {' ', '\t', '\r', '\n', '\0'};
    const size_t first = text.find_first_not_of(kSpaces);
    if (first == text.npos) return {};
    return text.substr(first, text.find_last_not_of(kSpaces) - first + 1);
}
