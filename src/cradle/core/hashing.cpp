#include <picosha2.h>

// TODO: Do the hashing more directly and eliminate this dependence.
#include <cradle/encodings/msgpack.hpp>

namespace cradle {

std::string
sha256_hash(dynamic const& value)
{
    return picosha2::hash256_hex_string(value_to_msgpack_string(value));
}

} // namespace cradle
