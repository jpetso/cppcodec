/**
 *  Copyright (C) 2015 Topology LP
 *  Copyright (C) 2018 Jakob Petsovits
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

#ifndef CPPCODEC_DETAIL_STREAM_CODEC
#define CPPCODEC_DETAIL_STREAM_CODEC

#include <array>
#include <limits>
#include <stdlib.h> // for abort()
#include <stdint.h>

#include "../parse_error.hpp"
#include "config.hpp"

namespace cppcodec {
namespace detail {

using alphabet_index_t = uint_fast16_t;

template <typename Codec, typename CodecVariant>
class stream_codec
{
public:
    template <typename Result, typename ResultState> static void encode(
            Result& encoded_result, ResultState&, const uint8_t* binary, size_t binary_size);

    template <typename Result, typename ResultState> static void decode(
            Result& binary_result, ResultState&, const char* encoded, size_t encoded_size);

    static constexpr size_t encoded_size(size_t binary_size) noexcept;
    static constexpr size_t decoded_max_size(size_t encoded_size) noexcept;
};

template <bool GeneratesPadding> // default for CodecVariant::generates_padding() == false
struct padder {
    template <typename CodecVariant, typename Result, typename ResultState, typename EncodedBlockSizeT>
    CPPCODEC_ALWAYS_INLINE void operator()(Result&, ResultState&, EncodedBlockSizeT) { }
};

template<> // specialization for CodecVariant::generates_padding() == true
struct padder<true> {
    template <typename CodecVariant, typename Result, typename ResultState, typename EncodedBlockSizeT>
    CPPCODEC_ALWAYS_INLINE void operator()(
            Result& encoded, ResultState& state, EncodedBlockSizeT num_padding_characters)
    {
        for (EncodedBlockSizeT i = 0; i < num_padding_characters; ++i) {
            data::put(encoded, state, CodecVariant::padding_symbol());
        }
    }
};

template <size_t I>
struct enc {
    // Block encoding: Go from 0 to (block size - 1), append a symbol for each iteration unconditionally.
    template <typename Codec, typename CodecVariant, typename Result, typename ResultState>
    static CPPCODEC_ALWAYS_INLINE void block(Result& encoded, ResultState& state, const uint8_t* src)
    {
        using EncodedBlockSizeT = decltype(Codec::encoded_block_size());
        constexpr static const EncodedBlockSizeT SymbolIndex = static_cast<EncodedBlockSizeT>(I - 1);

        enc<I - 1>().template block<Codec, CodecVariant>(encoded, state, src);
        data::put(encoded, state, CodecVariant::symbol(Codec::template index<SymbolIndex>(src)));
    }

    // Tail encoding: Go from 0 until (runtime) num_symbols, append a symbol for each iteration.
    template <typename Codec, typename CodecVariant, typename Result, typename ResultState,
            typename EncodedBlockSizeT = decltype(Codec::encoded_block_size())>
    static CPPCODEC_ALWAYS_INLINE void tail(
            Result& encoded, ResultState& state, const uint8_t* src, EncodedBlockSizeT num_symbols)
    {
        constexpr static const EncodedBlockSizeT SymbolIndex = Codec::encoded_block_size() - I;
        constexpr static const EncodedBlockSizeT NumSymbols = SymbolIndex + static_cast<EncodedBlockSizeT>(1);

        if (num_symbols == NumSymbols) {
            data::put(encoded, state, CodecVariant::symbol(Codec::template index_last<SymbolIndex>(src)));
            padder<CodecVariant::generates_padding()> pad;
#ifdef _MSC_VER
            pad.operator()<CodecVariant>(encoded, state, Codec::encoded_block_size() - NumSymbols);
#else
            pad.template operator()<CodecVariant>(encoded, state, Codec::encoded_block_size() - NumSymbols);
#endif
            return;
        }
        data::put(encoded, state, CodecVariant::symbol(Codec::template index<SymbolIndex>(src)));
        enc<I - 1>().template tail<Codec, CodecVariant>(encoded, state, src, num_symbols);
    }
};

template<> // terminating specialization
struct enc<0> {
    template <typename Codec, typename CodecVariant, typename Result, typename ResultState>
    static CPPCODEC_ALWAYS_INLINE void block(Result&, ResultState&, const uint8_t*) { }

    template <typename Codec, typename CodecVariant, typename Result, typename ResultState,
            typename EncodedBlockSizeT = decltype(Codec::encoded_block_size())>
    static CPPCODEC_ALWAYS_INLINE void tail(Result&, ResultState&, const uint8_t*, EncodedBlockSizeT)
    {
        abort(); // Not reached: block() should be called if num_symbols == block size, not tail().
    }
};

template <typename Codec, typename CodecVariant>
template <typename Result, typename ResultState>
inline void stream_codec<Codec, CodecVariant>::encode(
        Result& encoded_result, ResultState& state,
        const uint8_t* src, size_t src_size)
{
    using encoder = enc<Codec::encoded_block_size()>;

    const uint8_t* src_end = src + src_size;

    if (src_size >= Codec::binary_block_size()) {
        src_end -= Codec::binary_block_size();

        for (; src <= src_end; src += Codec::binary_block_size()) {
            encoder::template block<Codec, CodecVariant>(encoded_result, state, src);
        }
        src_end += Codec::binary_block_size();
    }

    if (src_end > src) {
        auto remaining_src_len = src_end - src;
        if (!remaining_src_len || remaining_src_len >= Codec::binary_block_size()) {
            abort();
            return;
        }
        auto num_symbols = Codec::num_encoded_tail_symbols(static_cast<uint8_t>(remaining_src_len));
        encoder::template tail<Codec, CodecVariant>(encoded_result, state, src, num_symbols);
    }
}

// CodecVariant::symbol() provides a symbol for an index.
// Use recursive templates to get the inverse lookup table for fast decoding.

template <typename T>
static const constexpr size_t num_possible_values()
{
    return static_cast<size_t>(
        static_cast<intmax_t>(std::numeric_limits<T>::max())
                - static_cast<intmax_t>(std::numeric_limits<T>::min()) + 1);
}

template <typename CodecVariant, alphabet_index_t InvalidIdx, size_t I>
struct index_if_in_alphabet {
    static CPPCODEC_ALWAYS_INLINE constexpr alphabet_index_t for_symbol(char symbol)
    {
        return (CodecVariant::symbol(
                    static_cast<alphabet_index_t>(CodecVariant::alphabet_size() - I)) == symbol)
            ? static_cast<alphabet_index_t>(CodecVariant::alphabet_size() - I)
            : index_if_in_alphabet<CodecVariant, InvalidIdx, I - 1>::for_symbol(symbol);
    }
};

template <typename CodecVariant, alphabet_index_t InvalidIdx>
struct index_if_in_alphabet<CodecVariant, InvalidIdx, 0> { // terminating specialization
    static CPPCODEC_ALWAYS_INLINE constexpr alphabet_index_t for_symbol(char symbol)
    {
        return InvalidIdx;
    }
};

template <typename CodecVariant, size_t I>
struct padding_searcher {
    static CPPCODEC_ALWAYS_INLINE constexpr bool exists_padding_symbol()
    {
        return CodecVariant::is_padding_symbol(
                CodecVariant::normalized_symbol(
                        static_cast<char>(num_possible_values<char>() - I)))
                || padding_searcher<CodecVariant, I - 1>::exists_padding_symbol();
    }
};

template <typename CodecVariant>
struct padding_searcher<CodecVariant, 0> { // terminating specialization
    static CPPCODEC_ALWAYS_INLINE constexpr bool exists_padding_symbol() { return false; }
};

template <typename CodecVariant>
struct alphabet_index_info
{
    static const constexpr size_t num_possible_symbols = num_possible_values<char>();

    using lookup_table_t = std::array<alphabet_index_t, num_possible_symbols>;

    static const constexpr alphabet_index_t padding_idx = 1 << 8;
    static const constexpr alphabet_index_t invalid_idx = 1 << 9;
    static const constexpr alphabet_index_t eof_idx = 1 << 10;

    static constexpr const bool padding_allowed = padding_searcher<
            CodecVariant, num_possible_symbols - 1>::exists_padding_symbol();

    static CPPCODEC_ALWAYS_INLINE constexpr bool allows_padding()
    {
        return padding_allowed;
    }
    static CPPCODEC_ALWAYS_INLINE constexpr bool is_padding(alphabet_index_t idx)
    {
        return allows_padding() ? (idx == padding_idx) : false;
    }
    static CPPCODEC_ALWAYS_INLINE constexpr bool is_invalid(alphabet_index_t idx) { return idx == invalid_idx; }
    static CPPCODEC_ALWAYS_INLINE constexpr bool is_eof(alphabet_index_t idx) { return idx == eof_idx; }
    static CPPCODEC_ALWAYS_INLINE constexpr bool is_stop_character(alphabet_index_t idx) { return idx & ~0xFF; }

    static CPPCODEC_ALWAYS_INLINE constexpr
    alphabet_index_t valid_index_or(alphabet_index_t a, alphabet_index_t b)
    {
        return a == invalid_idx ? b : a;
    }

    using idx_if_in_alphabet = index_if_in_alphabet<
            CodecVariant, invalid_idx, CodecVariant::alphabet_size()>;

    static CPPCODEC_ALWAYS_INLINE constexpr alphabet_index_t index_of(char symbol)
    {
        return valid_index_or(idx_if_in_alphabet::for_symbol(symbol),
            CodecVariant::is_eof_symbol(symbol) ? eof_idx
            : CodecVariant::is_padding_symbol(symbol) ? padding_idx
            : invalid_idx);
    }

    template <size_t... Syms>
    static CPPCODEC_ALWAYS_INLINE constexpr lookup_table_t make_lookup_table()
    {
        return lookup_table_t{{ alphabet_index_info::index_of(
            CodecVariant::normalized_symbol(static_cast<char>(Syms)))... }};
    }
};

template <typename CodecVariant>
class lookup_table_generator
{
    using IdxInfo = alphabet_index_info<CodecVariant>;

    template <size_t Sym, size_t... Symbols>
    struct gen {
        static CPPCODEC_ALWAYS_INLINE constexpr typename IdxInfo::lookup_table_t make()
        {
            // clang up to 3.6 has a limit of 256 recursive templates,
            // so pass a few more symbols at once.
            static_assert(Sym % 4 == 3,
                    "first symbol modulo 4 must be 3 to eventually become -1");
            return gen<Sym - 4, Sym - 3, Sym - 2, Sym - 1, Sym, Symbols...>::make();
        }
    };
    template <size_t... Symbols>
    struct gen<size_t(-1), Symbols...> { // terminating specialization
        static CPPCODEC_ALWAYS_INLINE constexpr typename IdxInfo::lookup_table_t make()
        {
            return IdxInfo::template make_lookup_table<Symbols...>();
        }
    };
public:
    static CPPCODEC_ALWAYS_INLINE constexpr typename IdxInfo::lookup_table_t make()
    {
        return gen<IdxInfo::num_possible_symbols - 1>::make();
    }
};

// At long last! The actual decode/encode functions.

template <typename Codec, typename CodecVariant>
template <typename Result, typename ResultState>
inline void stream_codec<Codec, CodecVariant>::decode(
        Result& binary_result, ResultState& state,
        const char* src_encoded, size_t src_size)
{
    static const auto alphabet_index_for_symbol =
            lookup_table_generator<CodecVariant>::make();

    const char* src = src_encoded;
    const char* src_end = src + src_size;

    alphabet_index_t alphabet_indexes[Codec::encoded_block_size()] = {};
    alphabet_index_t current_alphabet_index = alphabet_index_info<CodecVariant>::eof_idx;
    size_t current_block_index = 0;

    while (src < src_end) {
        if (CodecVariant::should_ignore(*src)) {
            ++src;
            continue;
        }
        current_alphabet_index = alphabet_index_for_symbol[*src];
        if (alphabet_index_info<CodecVariant>::is_stop_character(current_alphabet_index))
        {
            break;
        }
        ++src;
        alphabet_indexes[current_block_index++] = current_alphabet_index;

        if (current_block_index == Codec::encoded_block_size()) {
            Codec::decode_block(binary_result, state, alphabet_indexes);
            current_block_index = 0;
        }
    }

    if (alphabet_index_info<CodecVariant>::is_invalid(current_alphabet_index)) {
        throw symbol_error(*src);
    }
    ++src;

    size_t last_block_index = current_block_index;
    if (alphabet_index_info<CodecVariant>::is_padding(current_alphabet_index)) {
        if (current_block_index == 0) {
            // Don't accept padding at the start of a block.
            // The encoder should have omitted that padding altogether.
            throw padding_error();
        }
        // We're in here because we just read a (first) padding character. Try to read more.
        ++last_block_index;
        while (src < src_end) {
            alphabet_index_t last_alphabet_index = alphabet_index_for_symbol[*(src++)];

            if (alphabet_index_info<CodecVariant>::is_eof(last_alphabet_index)) {
                break;
            }
            if (!alphabet_index_info<CodecVariant>::is_padding(last_alphabet_index)) {
                throw padding_error();
            }

            ++last_block_index;
            if (last_block_index > Codec::encoded_block_size()) {
                throw padding_error();
            }
        }
    }

    if (last_block_index)  {
        if ((CodecVariant::requires_padding()
                    || alphabet_index_info<CodecVariant>::is_padding(current_alphabet_index)
                    ) && last_block_index != Codec::encoded_block_size())
        {
            // If the input is not a multiple of the block size then the input is incorrect.
            throw padding_error();
        }
        if (current_block_index >= Codec::encoded_block_size()) {
            abort();
            return;
        }
        Codec::decode_tail(binary_result, state, alphabet_indexes, current_block_index);
    }
}

template <typename Codec, typename CodecVariant>
inline constexpr size_t stream_codec<Codec, CodecVariant>::encoded_size(size_t binary_size) noexcept
{
    using C = Codec;

    // constexpr rules make this a lot harder to read than it actually is.
    return CodecVariant::generates_padding()
            // With padding, the encoded size is a multiple of the encoded block size.
            // To calculate that, round the binary size up to multiple of the binary block size,
            // then convert to encoded by multiplying with { base32: 8/5, base64: 4/3 }.
            ? (binary_size + (C::binary_block_size() - 1)
                    - ((binary_size + (C::binary_block_size() - 1)) % C::binary_block_size()))
                    * C::encoded_block_size() / C::binary_block_size()
            // No padding: only pad to the next multiple of 5 bits, i.e. at most a single extra byte.
            : (binary_size * C::encoded_block_size() / C::binary_block_size())
                    + (((binary_size * C::encoded_block_size()) % C::binary_block_size()) ? 1 : 0);
}

template <typename Codec, typename CodecVariant>
inline constexpr size_t stream_codec<Codec, CodecVariant>::decoded_max_size(size_t encoded_size) noexcept
{
    using C = Codec;

    return CodecVariant::requires_padding()
            ? (encoded_size / C::encoded_block_size() * C::binary_block_size())
            : (encoded_size / C::encoded_block_size() * C::binary_block_size())
                    + ((encoded_size % C::encoded_block_size())
                            * C::binary_block_size() / C::encoded_block_size());
}

} // namespace detail
} // namespace cppcodec

#endif // CPPCODEC_DETAIL_STREAM_CODEC
