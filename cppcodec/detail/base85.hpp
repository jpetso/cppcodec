/**
 *  Copyright (C) 2015 Topology LP
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

#ifndef CPPCODEC_DETAIL_BASE85
#define CPPCODEC_DETAIL_BASE85

#include <stdexcept>
#include <stdint.h>
#include <stdlib.h> // for abort()

#include "../data/access.hpp"
#include "../parse_error.hpp"
#include "config.hpp"
#include "stream_codec.hpp"

// Determine endianness on a best-effort basis. Use a fast path for big endian.
#if (defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN) || \
        (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || \
        defined(__BIG_ENDIAN__) || \
        defined(__ARMEB__) || \
        defined(__THUMBEB__) || \
        defined(__AARCH64EB__) || \
        defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
#define CPPCODEC_KNOWN_BIG_ENDIAN 1
#endif

#if (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) || \
        (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
        defined(__LITTLE_ENDIAN__) || \
        defined(__ARMEL__) || \
        defined(__THUMBEL__) || \
        defined(__AARCH64EL__) || \
        defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
#define CPPCODEC_KNOWN_LITTLE_ENDIAN 1
#endif

namespace cppcodec {
namespace detail {

template <typename CodecVariant>
struct base85_block_encoder
{
    template <typename Result, typename ResultState>
    static CPPCODEC_ALWAYS_INLINE void block(
            Result& encoded, ResultState& state, const uint8_t* src)
    {
#ifdef CPPCODEC_KNOWN_BIG_ENDIAN
        const uint32_t& value = *reinterpret_cast<const uint32_t*>(src);
#else
        uint_fast32_t value = (uint_fast32_t)(src[0]) << 24
                | (uint_fast32_t)(src[1]) << 16
                | (uint_fast32_t)(src[2]) << 8
                | src[3];
#endif
        data::put(encoded, state, CodecVariant::symbol((value / (85*85*85*85)) % 85));
        data::put(encoded, state, CodecVariant::symbol((value / (85*85*85)) % 85));
        data::put(encoded, state, CodecVariant::symbol((value / (85*85)) % 85));
        data::put(encoded, state, CodecVariant::symbol((value / 85) % 85));
        data::put(encoded, state, CodecVariant::symbol(value % 85));
    }

    template <typename Result, typename ResultState>
    static CPPCODEC_ALWAYS_INLINE void tail(
            Result& encoded, ResultState& state, const uint8_t* src, uint8_t num_symbols)
    {
        // Most (all?) variants don't use padding, so this should optimize out.
        using p = padder<CodecVariant::generates_padding()>;

        uint_fast32_t value = (uint_fast32_t)(src[0]) << 24;
        data::put(encoded, state, CodecVariant::symbol((value / (85*85*85*85)) % 85));
        data::put(encoded, state, CodecVariant::symbol((value / (85*85*85)) % 85));

        if (num_symbols == 2) { p::template pad<CodecVariant>(encoded, state, 3); return; }
        value |= (uint_fast32_t)(src[1]) << 16;
        data::put(encoded, state, CodecVariant::symbol((value / (85*85)) % 85));
        if (num_symbols == 3) { p::template pad<CodecVariant>(encoded, state, 2); return; }

        value |= (uint_fast32_t)(src[2]) << 8;
        data::put(encoded, state, CodecVariant::symbol((value / 85) % 85));
        if (num_symbols == 4) { p::template pad<CodecVariant>(encoded, state, 1); return; }

        abort(); // not reached: block() should be called for num_symbols == 5.
    }
};

struct base85_block_decoder
{
    template <typename Result, typename ResultState>
    static CPPCODEC_ALWAYS_INLINE void block(
            Result& decoded, ResultState& state, const uint8_t* idx)
    {
        uint32_t value = ((uint32_t)(idx[0]) * (85*85*85*85))
                + ((uint32_t)(idx[1]) * (85*85*85))
                + ((uint32_t)(idx[2]) * (85*85))
                + ((uint32_t)(idx[3]) * 85)
                + idx[4];
#if defined(CPPCODEC_KNOWN_BIG_ENDIAN)
        data::put(decoded, state, reinterpret_cast<uint8_t*>(&value)[0]);
        data::put(decoded, state, reinterpret_cast<uint8_t*>(&value)[1]);
        data::put(decoded, state, reinterpret_cast<uint8_t*>(&value)[2]);
        data::put(decoded, state, reinterpret_cast<uint8_t*>(&value)[3]);
#elif defined(CPPCODEC_KNOWN_LITTLE_ENDIAN)
        data::put(decoded, state, reinterpret_cast<uint8_t*>(&value)[3]);
        data::put(decoded, state, reinterpret_cast<uint8_t*>(&value)[2]);
        data::put(decoded, state, reinterpret_cast<uint8_t*>(&value)[1]);
        data::put(decoded, state, reinterpret_cast<uint8_t*>(&value)[0]);
#else
        data::put(decoded, state, (uint8_t)((value >> 24) & 0xFF));
        data::put(decoded, state, (uint8_t)((value >> 16) & 0xFF));
        data::put(decoded, state, (uint8_t)((value >> 8) & 0xFF));
        data::put(decoded, state, (uint8_t)(value & 0xFF));
#endif
    }

    template <typename Result, typename ResultState>
    static CPPCODEC_ALWAYS_INLINE void tail(
            Result& decoded, ResultState& state, const uint8_t* idx, size_t idx_len)
    {
        if (idx_len == 1) {
            throw invalid_input_length("invalid number of symbols in last base64 block: "
                    "found 1, expected 2, 3 or 4");
        }
        uint_fast32_t value = ((uint_fast32_t)(idx[0]) * (85*85*85*85))
                + ((uint_fast32_t)(idx[1]) * (85*85*85));

        data::put(decoded, state, (uint8_t)((value >> 24) & 0xFF));
        if (idx_len == 2) { return; } // decoded size 1
        value += (uint_fast32_t)(idx[2]) * (85*85);
        data::put(decoded, state, (uint8_t)((value >> 16) & 0xFF));
        if (idx_len == 3) { return; } // decoded size 2
        value += (uint_fast32_t)(idx[3]) * 85;
        data::put(decoded, state, (uint8_t)((value >> 8) & 0xFF));
        if (idx_len == 4) { return; } // decoded size 3

        throw invalid_input_length("invalid number of symbols in last base85 block");
    }
};

template <typename CodecVariant>
struct base85 : public CodecVariant::template codec_impl<base85<CodecVariant>>
{
    static inline constexpr uint8_t binary_block_size() { return 4; }
    static inline constexpr uint8_t encoded_block_size() { return 5; }

    using block_encoder = base85_block_encoder<CodecVariant>;
    using block_decoder = base85_block_decoder;

    static CPPCODEC_ALWAYS_INLINE constexpr uint8_t num_encoded_tail_symbols(uint8_t num_bytes)
    {
        return (num_bytes == 1) ? 2    // 2 symbols, 3 padding characters
                : (num_bytes == 2) ? 3 // 3 symbols, 2 padding characters
                : (num_bytes == 3) ? 4 // 4 symbols, 1 padding character
                : throw std::domain_error("invalid number of bytes in a tail block");
    }

    template <typename Result, typename ResultState>
    static void decode_block(Result& decoded, ResultState&, const uint8_t* idx);

    template <typename Result, typename ResultState>
    static void decode_tail(Result& decoded, ResultState&, const uint8_t* idx, size_t idx_len);
};

} // namespace detail
} // namespace cppcodec

#endif // CPPCODEC_DETAIL_BASE85
