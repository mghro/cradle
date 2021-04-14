#ifndef CRADLE_BACKGROUND_INTERNALS_HPP
#define CRADLE_BACKGROUND_INTERNALS_HPP

#include <atomic>
#include <condition_variable>
#include <cradle/background/job.h>
#include <cradle/background/os.h>
#include <cradle/background/system.h>
#include <cradle/io/http_requests.hpp>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

// This file defines various shared internals for the background execution
// system. For more information about the system in general, see system.hpp
// and api.hpp.

namespace cradle {

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
    unsigned version = 0;
    // jobs that might be ready to run
    job_priority_queue jobs;
    // jobs that have failed
    std::list<background_job_failure> failed_jobs;
    // this provides info about all jobs in the queue
    std::map<background_job_execution_data*, background_job_info> job_info;
    // for controlling access to the job queue
    std::mutex mutex;
    // for signalling when new jobs arrive
    std::condition_variable cv;
    // # of threads currently monitoring this queue for work
    size_t n_idle_threads = 0;
    // reported size of the queue
    // Internally, this is maintained as being the number of jobs in the jobs
    // queue that aren't marked as hidden.
    size_t reported_size = 0;
};

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

struct background_execution_system
{
    background_execution_pool
        pools[unsigned(background_job_queue_type::COUNT)];

    http_request_system http_system;
};

template<class ExecutionLoop>
void
add_background_thread(background_execution_pool& pool)
{
    auto data_proxy = std::make_shared<background_thread_data_proxy>();
    ExecutionLoop fn(pool.queue, data_proxy);
    auto thread
        = std::make_shared<background_execution_thread>(fn, data_proxy);
    pool.threads.push_back(thread);
    // lower_thread_priority(thread->thread);
}

struct background_job_execution_loop
{
    background_job_execution_loop(
        std::shared_ptr<background_job_queue> queue,
        std::shared_ptr<background_thread_data_proxy> data_proxy)
        : queue_(std::move(queue)), data_proxy_(std::move(data_proxy))
    {
    }
    void
    operator()();

 private:
    std::shared_ptr<background_job_queue> queue_;
    std::shared_ptr<background_thread_data_proxy> data_proxy_;
};

void
record_failure(
    background_job_execution_data& job,
    char const* msg,
    bool transient_failure);

struct http_request_processing_loop
{
    http_request_processing_loop(
        std::shared_ptr<background_job_queue> queue,
        std::shared_ptr<background_thread_data_proxy> data_proxy)
        : queue_(std::move(queue)), data_proxy_(std::move(data_proxy))
    //   ,
    //   connection_(std::make_shared<http_connection>())
    {
    }
    void
    operator()();

 private:
    std::shared_ptr<background_job_queue> queue_;
    std::shared_ptr<background_thread_data_proxy> data_proxy_;
    // std::shared_ptr<http_connection> connection_;
};

} // namespace detail

} // namespace cradle

#endif
