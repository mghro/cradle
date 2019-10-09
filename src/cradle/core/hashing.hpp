#ifndef CRADLE_CORE_HASHING_HPP
#define CRADLE_CORE_HASHING_HPP

#include <string>

namespace cradle {

struct dynamic;

std::string
sha256_hash(dynamic const& value);

} // namespace cradle

#endif
