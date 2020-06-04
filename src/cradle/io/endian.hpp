#ifndef CRADLE_IO_ENDIAN_HPP
#define CRADLE_IO_ENDIAN_HPP

#include <cradle/common.hpp>

// Although Boost already supplies an endian #define, it's considered an
// undocumented detail at this point, so for future compatibility, it's safer
// to use this one, which could be redirected if necessary.

#include <boost/detail/endian.hpp>

#ifdef BOOST_BIG_ENDIAN
#define CRADLE_BIG_ENDIAN
#endif
#ifdef BOOST_LITTLE_ENDIAN
#define CRADLE_LITTLE_ENDIAN
#endif

namespace cradle {

// Swap the endian of a single 8-bit word.
// This is defined so that generic code doesn't break if it attempts to use it.
inline void
swap_endian(uint8_t*)
{
}

// Swap the endian of a single 16-bit word.
void
swap_endian(uint16_t* word);

// Swap the endian of a single 32-bit word.
void
swap_endian(uint32_t* word);

// Swap the endian of a single 64-bit word.
void
swap_endian(uint64_t* word);

// Swap the endian of a single 8-bit word if the machine is little endian.
// This is defined so that generic code doesn't break if it attempts to use it.
inline void
swap_on_little_endian(uint8_t*)
{
}

// Swap the endian of a single 16-bit word if the machine is little endian.
void
swap_on_little_endian(uint16_t* word);

// Swap the endian of a single 32-bit word if the machine is little endian.
void
swap_on_little_endian(uint32_t* word);

// Swap the endian of a single 64-bit word if the machine is little endian.
void
swap_on_little_endian(uint64_t* word);

// Swap the endian of a single 8-bit word if the machine is big endian.
// This is defined so that generic code doesn't break if it attempts to use it.
inline void
swap_on_big_endian(uint8_t*)
{
}

// Swap the endian of a single 16-bit word if the machine is big endian.
void
swap_on_big_endian(uint16_t* word);

// Swap the endian of a single 32-bit word if the machine is big endian.
void
swap_on_big_endian(uint32_t* word);

// Swap the endian of a single 64-bit word if the machine is big endian.
void
swap_on_big_endian(uint64_t* word);

// Swap the endian of a single 16-bit word.
uint16_t
swap_uint16_endian(uint16_t word);

// Swap the endian of a single 32-bit word.
uint32_t
swap_uint32_endian(uint32_t word);

// Swap the endian of a single 64-bit word.
uint64_t
swap_uint64_endian(uint64_t word);

// Swap the endian of a single 16-bit word if the machine is little endian.
uint16_t
swap_uint16_on_little_endian(uint16_t word);

// Swap the endian of a single 32-bit word if the machine is little endian.
uint32_t
swap_uint32_on_little_endian(uint32_t word);

// Swap the endian of a single 64-bit word if the machine is little endian.
uint64_t
swap_uint64_on_little_endian(uint64_t word);

// Swap the endian of a single 16-bit word if the machine is big endian.
uint16_t
swap_uint16_on_big_endian(uint16_t word);

// Swap the endian of a single 32-bit word if the machine is big endian.
uint32_t
swap_uint32_on_big_endian(uint32_t word);

// Swap the endian of a single 64-bit word if the machine is big endian.
uint64_t
swap_uint64_on_big_endian(uint64_t word);

// Swap the endian on every word in the given 16-bit array.
// 'size' is the number of 16-bit words.
void
swap_array_endian(uint16_t* data, size_t size);

// Swap the endian on every word in the given 32-bit array.
// 'size' is the number of 32-bit words.
void
swap_array_endian(uint32_t* data, size_t size);

// Swap the endian on every word in the given 64-bit array.
// 'size' is the number of 64-bit words.
void
swap_array_endian(uint64_t* data, size_t size);

// On a little endian machine, swap the endian on every 16-bit word in the
// given block of data.  On a big endian machine, do nothing.
// 'size' is the number of 16-bit words.
void
swap_array_on_little_endian(uint16_t* data, size_t size);

// On a little endian machine, swap the endian on every 32-bit word in the
// given block of data.  On a big endian machine, do nothing.
// 'size' is the number of 32-bit words.
void
swap_array_on_little_endian(uint32_t* data, size_t size);

// On a little endian machine, swap the endian on every 64-bit word in the
// given block of data.  On a big endian machine, do nothing.
// 'size' is the number of 64-bit words.
void
swap_array_on_little_endian(uint64_t* data, size_t size);

// On a big endian machine, swap the endian on every 16-bit word in the given
// block of data.  On a little endian machine, do nothing.
// 'size' is the number of 16-bit words.
void
swap_array_on_big_endian(uint16_t* data, size_t size);

// On a big endian machine, swap the endian on every 32-bit word in the given
// block of data.  On a little endian machine, do nothing.
// 'size' is the number of 32-bit words.
void
swap_array_on_big_endian(uint32_t* data, size_t size);

// On a big endian machine, swap the endian on every 64-bit word in the given
// block of data.  On a little endian machine, do nothing.
// 'size' is the number of 64-bit words.
void
swap_array_on_big_endian(uint64_t* data, size_t size);

} // namespace cradle

#endif
