#ifndef CRADLE_BACKGROUND_JOB_H
#define CRADLE_BACKGROUND_JOB_H

#include <cradle/background/encoded_progress.h>
#include <cradle/core.h>

namespace cradle {

// This provides general information about a job.
struct background_job_info
{
    // TODO: Improve this...
    // - Add IDs?
    // - Add type/category?
    // - Switch to label plus longer, formatted description?
    string description;
};

// All jobs executed as part of a background system must implement this
// interface.
struct background_job_interface
{
    virtual ~background_job_interface()
    {
    }

    virtual void
    execute(
        check_in_interface& check_in, progress_reporter_interface& reporter)
        = 0;

    // virtual background_job_info
    // get_info() const = 0;
};

enum class background_job_state
{
    QUEUED,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELED
};

struct background_job_status
{
    background_job_state state = background_job_state::QUEUED;
    // Only valid if state is RUNNING, but still optional even then.
    optional<float> progress;
};

// This is defined in cradle/background/internals.h. It stores the actual data
// that the system tracks about a background job.
namespace detail {
struct background_job_execution_data;
}

typedef std::shared_ptr<detail::background_job_execution_data>
    background_job_ptr;

// A background_job_controller is used for monitoring and controlling the
// progress of a job.
struct background_job_controller
{
    background_job_controller()
    {
    }
    background_job_controller(background_job_ptr const& job) : job_(job)
    {
    }
    ~background_job_controller();

    void
    reset();

    bool
    is_valid() const
    {
        return job_ ? true : false;
    }

    // Cancel the job.
    void
    cancel();

    background_job_state
    state() const;

    // If state() is RUNNING, this is the job's progress.
    // (It's optional regardless. The job may not have reported progress.)
    optional<float>
    progress() const;

    background_job_ptr job_;
};

struct background_execution_system;

enum class background_job_queue_type
{
    // Calculation jobs are run in parallel according to the number of
    // available process cores.
    CALCULATION = 0,

    // Disk jobs are run with a much lower level of parallelism since it's
    // assumed that disk bandwidth is going to limit parallelism.
    DISK,

    // HTTP job are run with a very high level of parallelism.
    // (It would be essentially infinite, but we've determined that this can
    // cause issues with routers/ISPs getting upset when clients have too many
    // open connections.)
    HTTP,

    // Jobs in the following queues are long-lived network request jobs that
    // may run indefinitely but consume very little bandwidth, so they each get
    // their own thread.
    NOTIFICATION_WATCH,
    REMOTE_CALCULATION,

    // This is just here to capture the count of queue types.
    COUNT
};

CRADLE_DEFINE_FLAG_TYPE(background_job)
// Don't include this job (by default) in reports about what jobs are running
// in the system.
CRADLE_DEFINE_FLAG(background_job, 0b01, BACKGROUND_JOB_HIDDEN)
// This job should run even if no external entities are interested in it.
CRADLE_DEFINE_FLAG(background_job, 0b10, BACKGROUND_JOB_INDEPENDENT)

// Add a job for the background execution system to execute.
//
// The returned
//
// :priority controls the priority of the job. A higher number means higher
// priority. Negative numbers are OK, and 0 is taken to be the default/neutral
// priority.
//
background_job_controller
add_background_job(
    background_execution_system& system,
    background_job_queue_type queue,
    std::unique_ptr<background_job_interface> job,
    background_job_flag_set flags = NO_FLAGS,
    int priority = 0);

} // namespace cradle

#endif
