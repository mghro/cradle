#ifndef CRADLE_CACHING_IMMUTABLE_PRODUCTION_H
#define CRADLE_CACHING_IMMUTABLE_PRODUCTION_H

#include <cradle/background/encoded_progress.h>
#include <cradle/core.h>
#include <cradle/core/id.h>

namespace cradle {

struct immutable_cache;

// report_immutable_cache_loading_progress() is used by producers to report
// progress made in computing individual cache results.
void
report_immutable_cache_loading_progress(
    immutable_cache& cache, id_interface const& key, float progress);

// set_immutable_cache_data() is used by producers to transmit the data that
// they produce to the immutable cache.

void
set_immutable_cache_data(
    immutable_cache& cache, id_interface const& key, untyped_immutable value);

template<class T>
void
set_immutable_cache_data(
    immutable_cache& cache, id_interface const& key, immutable<T> const& value)
{
    set_immutable_cache_data(cache, key, erase_type(value));
}

// report_immutable_cache_loading_failure() is used by producers to report
// failures in their attempts to compute individual cache results.
void
report_immutable_cache_loading_failure(
    immutable_cache& cache, id_interface const& key);

} // namespace cradle

#endif
