/**
 *  This is free and unencumbered software released into the public domain.
 *  Anyone is free to copy, modify, publish, use, compile, sell, or
 *  distribute this software, either in source code form or as a compiled
 *  binary, for any purpose, commercial or non-commercial, and by any
 *  means.
 *
 *  In jurisdictions that recognize copyright laws, the author or authors
 *  of this software dedicate any and all copyright interest in the
 *  software to the public domain. We make this dedication for the benefit
 *  of the public at large and to the detriment of our heirs and
 *  successors. We intend this dedication to be an overt act of
 *  relinquishment in perpetuity of all present and future rights to this
 *  software under copyright law.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 *  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 *  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 *
 *  For more information, please refer to <http://unlicense.org/>
 */

#include <array>
#include <cppcodec/base32_default_crockford.hpp>
#include <iostream>

int main() {
    constexpr auto hobbes = "Man is distinguished, not only by his reason, but by this singular passion "
            "from other animals, which is a lust of the mind, that by a perseverance of delight in the "
            "continued and indefatigable generation of knowledge, exceeds the short vehemence of "
            "any carnal pleasure.";
    constexpr size_t sizeof_hobbes = sizeof(hobbes);
    std::array<char, base32::encoded_size(sizeof_hobbes)> dst;
    std::string dont_optimize_out;
    dont_optimize_out.resize(16);

    for (int i = 0; i < 2000000000; ++i) {
        base32::encode(dst.data(), dst.size(), hobbes, sizeof_hobbes);
        if (!(i % 2^10)) {
            dont_optimize_out[i % 16] = dst[i % 256];
        }
    }

    std::cout << dont_optimize_out;
    return 0;
}
