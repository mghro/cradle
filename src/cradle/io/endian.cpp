#include <algorithm>
#include <cradle/io/endian.hpp>

namespace cradle {

void
swap_endian(uint16_t* word)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(word);
    std::swap(*p, *(p + 1));
}

void
swap_endian(uint32_t* word)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(word);
    std::swap(*p, *(p + 3));
    std::swap(*(p + 1), *(p + 2));
}

void
swap_endian(uint64_t* word)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(word);
    std::swap(*p, *(p + 7));
    std::swap(*(p + 1), *(p + 6));
    std::swap(*(p + 2), *(p + 5));
    std::swap(*(p + 3), *(p + 4));
}

void
swap_on_little_endian(uint16_t* word)
{
#ifdef CRADLE_LITTLE_ENDIAN
    return swap_endian(word);
#endif
}

void
swap_on_little_endian(uint32_t* word)
{
#ifdef CRADLE_LITTLE_ENDIAN
    return swap_endian(word);
#endif
}

void
swap_on_little_endian(uint64_t* word)
{
#ifdef CRADLE_LITTLE_ENDIAN
    return swap_endian(word);
#endif
}

void
swap_on_big_endian(uint16_t* word)
{
#ifdef CRADLE_BIG_ENDIAN
    return swap_endian(word);
#endif
}

void
swap_on_big_endian(uint32_t* word)
{
#ifdef CRADLE_BIG_ENDIAN
    return swap_endian(word);
#endif
}

void
swap_on_big_endian(uint64_t* word)
{
#ifdef CRADLE_BIG_ENDIAN
    return swap_endian(word);
#endif
}

uint16_t
swap_uint16_endian(uint16_t word)
{
    swap_endian(&word);
    return word;
}

uint32_t
swap_uint32_endian(uint32_t word)
{
    swap_endian(&word);
    return word;
}

uint64_t
swap_uint64_endian(uint64_t word)
{
    swap_endian(&word);
    return word;
}

uint16_t
swap_uint16_on_little_endian(uint16_t word)
{
#ifdef CRADLE_LITTLE_ENDIAN
    return swap_uint16_endian(word);
#else
    return word;
#endif
}

uint32_t
swap_uint32_on_little_endian(uint32_t word)
{
#ifdef CRADLE_LITTLE_ENDIAN
    return swap_uint32_endian(word);
#else
    return word;
#endif
}

uint64_t
swap_uint64_on_little_endian(uint64_t word)
{
#ifdef CRADLE_LITTLE_ENDIAN
    return swap_uint64_endian(word);
#else
    return word;
#endif
}

uint16_t
swap_uint16_on_big_endian(uint16_t word)
{
#ifdef CRADLE_BIG_ENDIAN
    return swap_uint16_endian(word);
#else
    return word;
#endif
}

uint32_t
swap_uint32_on_big_endian(uint32_t word)
{
#ifdef CRADLE_BIG_ENDIAN
    return swap_uint32_endian(word);
#else
    return word;
#endif
}

uint64_t
swap_uint64_on_big_endian(uint64_t word)
{
#ifdef CRADLE_BIG_ENDIAN
    return swap_uint64_endian(word);
#else
    return word;
#endif
}

void
swap_array_endian(uint16_t* data, size_t size)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(data);
    uint8_t* end = reinterpret_cast<uint8_t*>(data + size);
    for (; p != end; p += 2)
        std::swap(*p, *(p + 1));
}

void
swap_array_endian(uint32_t* data, size_t size)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(data);
    uint8_t* end = reinterpret_cast<uint8_t*>(data + size);
    for (; p != end; p += 4)
    {
        std::swap(*p, *(p + 3));
        std::swap(*(p + 1), *(p + 2));
    }
}

void
swap_array_endian(uint64_t* data, size_t size)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(data);
    uint8_t* end = reinterpret_cast<uint8_t*>(data + size);
    for (; p != end; p += 8)
    {
        std::swap(*p, *(p + 7));
        std::swap(*(p + 1), *(p + 6));
        std::swap(*(p + 2), *(p + 5));
        std::swap(*(p + 3), *(p + 4));
    }
}

void
swap_array_on_little_endian(uint16_t* data, size_t size)
{
#ifdef CRADLE_LITTLE_ENDIAN
    swap_array_endian(data, size);
#endif
}

void
swap_array_on_little_endian(uint32_t* data, size_t size)
{
#ifdef CRADLE_LITTLE_ENDIAN
    swap_array_endian(data, size);
#endif
}

void
swap_array_on_little_endian(uint64_t* data, size_t size)
{
#ifdef CRADLE_LITTLE_ENDIAN
    swap_array_endian(data, size);
#endif
}

void
swap_array_on_big_endian(uint16_t* data, size_t size)
{
#ifdef CRADLE_BIG_ENDIAN
    swap_array_endian(data, size);
#endif
}

void
swap_array_on_big_endian(uint32_t* data, size_t size)
{
#ifdef CRADLE_BIG_ENDIAN
    swap_array_endian(data, size);
#endif
}

void
swap_array_on_big_endian(uint64_t* data, size_t size)
{
#ifdef CRADLE_BIG_ENDIAN
    swap_array_endian(data, size);
#endif
}

} // namespace cradle
