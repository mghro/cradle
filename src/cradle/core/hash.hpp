#ifndef CRADLE_CORE_HASH_HPP
#define CRADLE_CORE_HASH_HPP

#include <functional>

#include <boost/functional/hash.hpp>

namespace cradle {

template<class T>
std::size_t
invoke_hash(T const& x)
{
    return boost::hash<T>()(x);
}

} // namespace cradle

#endif
