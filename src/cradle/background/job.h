#ifndef CRADLE_BACKGROUND_JOB_H
#define CRADLE_BACKGROUND_JOB_H

#include <cradle/background/encoded_progress.h>
#include <cradle/core.h>

namespace cradle {

// This provides general information about a job.
struct background_job_info
{
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
    gather_inputs()
    {
    }

    virtual bool
    inputs_ready()
    {
        return true;
    }

    virtual void
    execute(
        check_in_interface& check_in, progress_reporter_interface& reporter)
        = 0;

    virtual background_job_info
    get_info() const = 0;
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
    encoded_optional_progress progress;
};

// This is defined in cradle/background/internals.h. It stores the actual data
// that the system tracks about a background job.
struct background_job_execution_data;

typedef std::shared_ptr<background_job_execution_data> background_job_ptr;

// A background_job_controller is used for monitoring and controlling the
// progress of a job.
struct background_job_controller : noncopyable
{
    background_job_controller()
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

// Add a job for the background execution system to execute.
//
// If :controller is nullptr, it's ignored.
// (This implies there is no external entity directly controlling the job.)
//
// :priority controls the priority of the job. A higher number means higher
// priority. Negative numbers are OK, and 0 is taken to be the default/neutral
// priority.
//
// void
// add_background_job(
//     background_execution_system& system,
//     background_job_queue_type queue,
//     background_job_controller* controller,
//     background_job_interface* job,
//     background_job_flag_set flags = NO_FLAGS,
//     int priority = 0);

} // namespace cradle

#endif
