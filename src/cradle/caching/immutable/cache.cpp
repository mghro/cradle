#include <cradle/caching/immutable/cache.hpp>

#include <cradle/caching/immutable/internals.h>

namespace cradle {

immutable_cache::immutable_cache()
    : impl(std::make_unique<detail::immutable_cache>())
{
}

immutable_cache::~immutable_cache() = default;

} // namespace cradle
