#ifndef CRADLE_ENCODINGS_MSGPACK_H
#define CRADLE_ENCODINGS_MSGPACK_H

#include <cradle/core.h>

// This file provides functions for converting dynamic values to and from
// MessagePack.

namespace cradle {

dynamic
parse_msgpack_value(uint8_t const* data, size_t size);

dynamic
parse_msgpack_value(string const& msgpack);

// This form takes a separate parameter that provides ownership of the data
// buffer. This allows the parser to store blobs by pointing into the original
// data rather than copying them.
dynamic
parse_msgpack_value(
    ownership_holder const& ownership, uint8_t const* data, size_t size);

string
value_to_msgpack_string(dynamic const& v);

blob
value_to_msgpack_blob(dynamic const& v);

CRADLE_DEFINE_EXCEPTION(msgpack_blob_size_limit_exceeded)
CRADLE_DEFINE_ERROR_INFO(uint64_t, msgpack_blob_size)
CRADLE_DEFINE_ERROR_INFO(uint64_t, msgpack_blob_size_limit)

} // namespace cradle

#endif
