#ifndef CRADLE_BACKGROUND_JOB_H
#define CRADLE_BACKGROUND_JOB_H

#include <atomic>

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

CRADLE_DEFINE_FLAG_TYPE(background_job)
// Don't include this job (by default) in reports about what jobs are running
// in the system.
CRADLE_DEFINE_FLAG(background_job, 0b01, BACKGROUND_JOB_HIDDEN)
// Ensure that an idle thread exists to pick up the job.
CRADLE_DEFINE_FLAG(background_job, 0b01, BACKGROUND_JOB_SKIP_QUEUE)

namespace detail {

struct background_job_execution_data : noncopyable
{
    background_job_execution_data(
        std::unique_ptr<background_job_interface> job,
        background_job_flag_set flags,
        int priority)
        : job(std::move(job)),
          flags(flags),
          priority(priority),
          state(background_job_state::QUEUED),
          cancel(false)
    {
    }

    // the job itself, owned by this structure
    std::unique_ptr<background_job_interface> job;

    // the flags and priority level supplied by whoever created the job
    background_job_flag_set flags;
    int priority;

    // the current state of the job
    std::atomic<background_job_state> state;
    // the progress of the job
    std::atomic<encoded_optional_progress> progress;

    // cancellation flag - If this is set, the job will be canceled next time
    // it checks in.
    std::atomic<bool> cancel;
};

} // namespace detail

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

} // namespace cradle

#endif
