#ifndef CRADLE_BACKGROUND_INTERNALS_HPP
#define CRADLE_BACKGROUND_INTERNALS_HPP

#include <condition_variable>
#include <cradle/background/job.h>
#include <cradle/background/os.h>
#include <cradle/background/system.h>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

// This file defines various shared internals for the background execution
// system. For more information about the system in general, see system.hpp
// and api.hpp.

namespace cradle {

struct background_job_execution_data : noncopyable
{
    background_job_execution_data(
        std::unique_ptr<background_job_interface>&& job,
        int priority,
        bool hidden)
        : job(std::move(job)),
          priority(priority),
          state(background_job_state::QUEUED),
          cancel(false),
          hidden(hidden)
    {
    }

    // the job itself, owned by this structure
    std::unique_ptr<background_job_interface> job;

    // If this is true, the job won't be included in status reports.
    bool hidden;

    int priority;

    // the current state of the job
    std::atomic<background_job_state> state;
    // the progress of the job
    std::atomic<encoded_optional_progress> progress;

    // If this is set, the job will be canceled next time it checks in.
    std::atomic<bool> cancel;
};

struct background_job_sorter
{
    bool
    operator()(background_job_ptr const& a, background_job_ptr const& b)
    {
        return a->priority > b->priority;
    }
};

struct background_job_canceled
{
};

struct background_job_check_in : check_in_interface
{
    background_job_check_in(background_job_ptr const& job) : job(job)
    {
    }
    void
    operator()()
    {
        if (job->cancel)
        {
            job->state = background_job_state::CANCELED;
            throw background_job_canceled();
        }
    }
    background_job_ptr job;
};

struct background_job_progress_reporter : progress_reporter_interface
{
    background_job_progress_reporter(background_job_ptr const& job) : job(job)
    {
    }
    void
    operator()(float progress)
    {
        job->progress.store(encode_progress(progress));
    }
    background_job_ptr job;
};

typedef std::priority_queue<
    background_job_ptr,
    std::vector<background_job_ptr>,
    background_job_sorter>
    job_priority_queue;

struct background_job_failure
{
    // the job that failed
    background_job_ptr job;
    // Was it a transient failure?
    // This indicates whether or not it's worth retrying the job.
    bool is_transient;
    // the associated error message
    string message;
};

struct background_job_queue : noncopyable
{
    // used to track changes in the queue
    unsigned version;
    // jobs that might be ready to run
    job_priority_queue jobs;
    // jobs that are waiting on dependencies
    job_priority_queue waiting_jobs;
    // counts how many times jobs have been woken up
    size_t wake_up_counter;
    // jobs that have failed
    std::list<background_job_failure> failed_jobs;
    // this provides info about all jobs in the queue
    std::map<background_job_execution_data*, background_job_info> job_info;
    // for controlling access to the job queue
    std::mutex mutex;
    // for signalling when new jobs arrive
    std::condition_variable cv;
    // # of threads currently monitoring this queue for work
    size_t n_idle_threads;
    // reported size of the queue
    // Internally, this is maintained as being the number of jobs in either
    // the jobs queue or the waiting_jobs queue that aren't marked as hidden.
    size_t reported_size;

    background_job_queue()
        : wake_up_counter(0), n_idle_threads(0), reported_size(0)
    {
    }
};

// Move all jobs in the waiting queue back to the main queue.
void
wake_up_waiting_jobs(background_job_queue& queue);

// This is used for communication between the threads in a thread pool and
// outside entities.
struct background_thread_data_proxy
{
    // protects access to this data
    std::mutex mutex;
    // the job currently being executed in this thread (if any)
    background_job_ptr active_job;
};

struct background_execution_thread
{
    background_execution_thread()
    {
    }

    template<class Function>
    background_execution_thread(
        Function function,
        std::shared_ptr<background_thread_data_proxy> const& data_proxy)
        : thread(function), data_proxy(data_proxy)
    {
    }

    std::thread thread;

    std::shared_ptr<background_thread_data_proxy> data_proxy;
};

// A background_execution_pool combines a queue of jobs with a pool of threads
// that are intended to execute those jobs.
struct background_execution_pool
{
    std::shared_ptr<background_job_queue> queue;
    std::vector<std::shared_ptr<background_execution_thread>> threads;
};

} // namespace cradle

#endif
