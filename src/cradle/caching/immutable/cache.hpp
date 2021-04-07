#ifndef CRADLE_CACHING_IMMUTABLE_CACHE_HPP
#define CRADLE_CACHING_IMMUTABLE_CACHE_HPP

#include <memory>

namespace cradle {

namespace detail {

struct immutable_cache;

} // namespace detail

struct immutable_cache
{
    immutable_cache();
    ~immutable_cache();

    std::unique_ptr<detail::immutable_cache> impl;
};

} // namespace cradle

#endif
