#include <cradle/encodings/base64.hpp>

#include <boost/scoped_array.hpp>
#include <iomanip>
#include <sstream>

namespace cradle {

size_t
get_base64_encoded_length(size_t raw_length)
{
    return (raw_length + 2) / 3 * 4 + 1;
}

size_t
get_base64_decoded_length(size_t encoded_length)
{
    return (encoded_length + 3) / 4 * 3;
}

void
base64_encode(
    char* dst,
    size_t* dst_size,
    uint8_t const* src,
    size_t src_size,
    base64_character_set const& character_set)
{
    uint8_t const* src_end = src + src_size;
    char const* dst_start = dst;
    while (1)
    {
        if (src == src_end)
            break;
        int n = *src;
        ++src;
        *dst++ = character_set.digits[(n >> 2) & 63];
        n <<= 8;
        if (src != src_end)
            n |= *src;
        *dst++ = character_set.digits[(n >> 4) & 63];
        if (src == src_end)
        {
            *dst++ = character_set.padding;
            *dst++ = character_set.padding;
            break;
        }
        ++src;
        n <<= 8;
        if (src != src_end)
            n |= *src;
        *dst++ = character_set.digits[(n >> 6) & 63];
        if (src == src_end)
        {
            *dst++ = character_set.padding;
            break;
        }
        ++src;
        *dst++ = character_set.digits[(n >> 0) & 63];
    }
    *dst = 0;
    *dst_size = dst - dst_start;
}

string
base64_encode(
    uint8_t const* src,
    size_t src_size,
    base64_character_set const& character_set)
{
    boost::scoped_array<char> dst(
        new char[get_base64_encoded_length(src_size)]);
    size_t dst_size;
    base64_encode(dst.get(), &dst_size, src, src_size, character_set);
    return string(dst.get());
}

string
base64_encode(string const& source, base64_character_set const& character_set)
{
    return base64_encode(
        reinterpret_cast<uint8_t const*>(&source[0]),
        source.length(),
        character_set);
}

void
base64_decode(
    uint8_t* dst,
    size_t* dst_size,
    char const* src,
    size_t src_size,
    base64_character_set const& character_set)
{
    uint8_t reverse_mapping[0x100];
    for (int i = 0; i != 0x100; ++i)
        reverse_mapping[i] = 0xff;
    for (uint8_t i = 0; i != 64; ++i)
        reverse_mapping[uint8_t(character_set.digits[i])] = i;

    char const* src_begin = src;
    char const* src_end = src + src_size;
    uint8_t const* dst_start = dst;

    while (1)
    {
        if (src == src_end)
            break;

        uint8_t c0 = reverse_mapping[uint8_t(*src)];
        ++src;
        if (c0 > 63 || src == src_end)
        {
            CRADLE_THROW(
                parsing_error()
                << expected_format_info("base64")
                << parsed_text_info(string(src_begin, src_end)));
        }

        uint8_t c1 = reverse_mapping[uint8_t(*src)];
        ++src;
        if (c1 > 63)
        {
            CRADLE_THROW(
                parsing_error()
                << expected_format_info("base64")
                << parsed_text_info(string(src_begin, src_end)));
        }

        *dst++ = (c0 << 2) | (c1 >> 4);

        if (src == src_end || *src == character_set.padding)
            break;

        uint8_t c2 = reverse_mapping[uint8_t(*src)];
        ++src;
        if (c2 > 63)
        {
            CRADLE_THROW(
                parsing_error()
                << expected_format_info("base64")
                << parsed_text_info(string(src_begin, src_end)));
        }

        *dst++ = ((c1 & 0xf) << 4) | (c2 >> 2);

        if (src == src_end || *src == character_set.padding)
            break;

        uint8_t c3 = reverse_mapping[uint8_t(*src)];
        ++src;
        if (c3 > 63)
        {
            CRADLE_THROW(
                parsing_error()
                << expected_format_info("base64")
                << parsed_text_info(string(src_begin, src_end)));
        }

        *dst++ = ((c2 & 0x3) << 6) | c3;
    }
    *dst_size = dst - dst_start;
}

string
base64_decode(string const& encoded, base64_character_set const& character_set)
{
    boost::scoped_array<char> decoded(
        new char[get_base64_decoded_length(encoded.length())]);
    size_t decoded_size;
    base64_decode(
        reinterpret_cast<uint8_t*>(decoded.get()),
        &decoded_size,
        &encoded[0],
        encoded.length(),
        character_set);
    return string(decoded.get(), decoded.get() + decoded_size);
}

} // namespace cradle
