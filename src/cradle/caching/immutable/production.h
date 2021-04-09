#ifndef CRADLE_CACHING_IMMUTABLE_PRODUCTION_H
#define CRADLE_CACHING_IMMUTABLE_PRODUCTION_H

#include <cradle/background/encoded_progress.h>
#include <cradle/core.h>
#include <cradle/core/id.h>

namespace cradle {

struct immutable_cache;

// report_immutable_cache_loading_progress() is used by background jobs to
// report progress made in computing individual cache results.
void
report_immutable_cache_loading_progress(
    immutable_cache& cache, id_interface const& key, float progress);

// set_immutable_cache_data() is used by background jobs to transmit the data
// that they produce into the background caching system.
void
set_immutable_cache_data(
    immutable_cache& cache,
    id_interface const& key,
    untyped_immutable&& value);

} // namespace cradle

#endif
