/**
 *  Copyright (C) 2016 Topology LP
 *  All rights reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

#ifndef CPPCODEC_BASE85_Z85
#define CPPCODEC_BASE85_Z85

#include "detail/codec.hpp"
#include "detail/base85.hpp"

namespace cppcodec {

namespace detail {

static constexpr const char base85_z85_alphabet[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '.', '-', ':', '+', '=', '^', '!', '/', '*', '?', '&', '<', '>',
    '(', ')', '[', ']', '{', '}', '@', '%', '$', '#'
};
static_assert(sizeof(base85_z85_alphabet) == 85, "base85 alphabet must have 85 values");

class base85_z85
{
public:
    template <typename Codec> using codec_impl = stream_codec<Codec, base85_z85>;

    static inline constexpr bool generates_padding() { return false; }
    static inline constexpr bool requires_padding() { return false; }
    static inline constexpr bool is_padding_symbol(char /*c*/) { return false; }

    static inline constexpr char symbol(uint8_t index)
    {
        return base85_z85_alphabet[index];
    }

    static inline constexpr uint8_t index_of(char c)
    {
        return (c >= '0' && c <= '9') ? (c - '0')
                : (c >= 'a' && c <= 'z') ? (c - 'a' + 10)
                : (c >= 'A' && c <= 'Z') ? (c - 'A' + 36)
                : (c == '.') ? 62
                : (c == '-') ? 63
                : (c == ':') ? 64
                : (c == '+') ? 65
                : (c == '=') ? 66
                : (c == '^') ? 67
                : (c == '!') ? 68
                : (c == '/') ? 69
                : (c == '*') ? 70
                : (c == '?') ? 71
                : (c == '&') ? 72
                : (c == '<') ? 73
                : (c == '>') ? 74
                : (c == '(') ? 75
                : (c == ')') ? 76
                : (c == '[') ? 77
                : (c == ']') ? 78
                : (c == '{') ? 79
                : (c == '}') ? 80
                : (c == '@') ? 81
                : (c == '%') ? 82
                : (c == '$') ? 83
                : (c == '#') ? 84
                : (c == '\0') ? 255 // stop at end of string
                : throw symbol_error(c);
    }

    static inline constexpr bool should_ignore(uint8_t index) { return index == 253; }
    static inline constexpr bool is_special_character(uint8_t index) { return index > 85; }
    static inline constexpr bool is_eof(uint8_t index) { return index == 255; }
};

} // namespace detail

using base85_z85 = detail::codec<detail::base85<detail::base85_z85>>;

} // namespace cppcodec

#endif // CPPCODEC_BASE85_Z85
