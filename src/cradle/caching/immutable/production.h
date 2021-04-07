#ifndef CRADLE_CACHING_IMMUTABLE_PRODUCTION_H
#define CRADLE_CACHING_IMMUTABLE_PRODUCTION_H

#include <cradle/core.h>
#include <cradle/background/encoded_progress.h>
#include <cradle/core/id.h>

namespace cradle {

enum class immutable_cache_job_state
{
    QUEUED,
    RUNNING,
    COMPLETED,
    FAILED
};

struct immutable_cache_job_status
{
    immutable_cache_job_state state = immutable_cache_job_state::QUEUED;
    // Only valid if state is RUNNING, but still optional even then.
    encoded_optional_progress progress;
};

struct immutable_cache_job_interface
{
    // If this is invoked before the job completes, the job should assume that
    // there is no longer any interest in its result and should cancel itself
    // if possible.
    virtual ~immutable_cache_job_interface()
    {
    }

    // Get the status of the job.
    virtual immutable_cache_job_status
    status() const = 0;
};

} // namespace cradle

#endif
