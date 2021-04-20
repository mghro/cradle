#ifndef CRADLE_BACKGROUND_EXECUTION_POOL_H
#define CRADLE_BACKGROUND_EXECUTION_POOL_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include <cradle/background/job.h>

// This file defines CRADLE's system for executing background jobs in thread
// pools. As with most of the rest of the CRADLE background system, this is a
// pretty straightforward port of the Astroid code, and it's likely to be
// replaced by standard C++ mechanisms once those are finalized.

namespace cradle {

namespace detail {

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
    // flag to tell active threads that the queue is shutting down
    bool terminating = false;
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

    background_execution_thread(
        std::thread thread,
        std::shared_ptr<background_thread_data_proxy> data_proxy)
        : thread(std::move(thread)), data_proxy(std::move(data_proxy))
    {
    }

    std::thread thread;
    std::shared_ptr<background_thread_data_proxy> data_proxy;
};

template<class Executor>
struct background_execution_loop
{
    background_execution_loop(
        std::shared_ptr<background_job_queue> queue,
        std::shared_ptr<background_thread_data_proxy> data_proxy,
        Executor&& executor)
        : queue_(std::move(queue)),
          data_proxy_(std::move(data_proxy)),
          executor_(std::move(executor))
    {
    }

    void
    operator()()
    {
        while (1)
        {
            auto& queue = *queue_;

            // Wait until the queue has a job in it, and then grab the job.
            background_job_ptr job;
            {
                std::unique_lock<std::mutex> lock(queue.mutex);
                ++queue.version;
                ++queue.n_idle_threads;
                // TODO: If this queue is allocating threads on demand and
                // there are already a lot of idle threads, just end this one.

                while (queue.jobs.empty())
                {
                    queue.cv.wait(lock);
                    if (queue.terminating)
                        return;
                }
                job = queue.jobs.top();
                ++queue.version;
                queue.jobs.pop();
                if (!(job->flags & BACKGROUND_JOB_HIDDEN))
                    --queue.reported_size;
                --queue.n_idle_threads;

                // If it's already been instructed to cancel, cancel it.
                if (job->cancel)
                {
                    job->state = background_job_state::CANCELED;
                    queue.job_info.erase(&*job);
                    continue;
                }
            }

            {
                std::scoped_lock<std::mutex> lock(data_proxy_->mutex);
                data_proxy_->active_job = job;
            }

            try
            {
                job->state = background_job_state::RUNNING;
                background_job_check_in check_in(job);
                background_job_progress_reporter reporter(job);
                executor_.execute(check_in, reporter, *job->job);
                job->state = background_job_state::COMPLETED;
            }
            catch (background_job_canceled&)
            {
            }
            catch (boost::exception&)
            {
                // string msg = "(bjc) " + job->job->get_info().description
                //              + string("\n") + string(e.what());
                // record_failure(queue, job, msg, e.is_transient());
            }
            catch (std::bad_alloc&)
            {
                // string msg = "(bj) " + job->job->get_info().description
                //              + string("\n") + string(" out of memory");
                // record_failure(queue, job, msg, true);
            }
            catch (std::exception&)
            {
                // string msg = "(bjs) " + job->job->get_info().description
                //              + string("\n") + string(e.what());
                // record_failure(queue, job, msg, false);
            }
            catch (...)
            {
                // string msg = "(bj) " + job->job->get_info().description;
                // record_failure(queue, job, msg, false);
            }

            {
                std::scoped_lock<std::mutex> lock(queue.mutex);
                queue.job_info.erase(&*job);
                ++queue.version;
            }

            {
                std::scoped_lock<std::mutex> lock(data_proxy_->mutex);
                data_proxy_->active_job.reset();
            }
        }
    }

 private:
    std::shared_ptr<background_job_queue> queue_;
    std::shared_ptr<background_thread_data_proxy> data_proxy_;
    Executor executor_;
};

// A background_execution_pool combines a queue of jobs with a pool of
// threads that are intended to execute those jobs.
struct background_execution_pool : noncopyable
{
    std::shared_ptr<background_job_queue> queue;
    std::vector<std::shared_ptr<background_execution_thread>> threads;
    std::function<std::thread(
        std::shared_ptr<background_job_queue>,
        std::shared_ptr<background_thread_data_proxy>)>
        create_thread;
};

void
add_background_thread(background_execution_pool& pool);

template<class Executor, class CreateExecutor>
void
initialize_pool(
    background_execution_pool& pool,
    size_t initial_thread_count,
    CreateExecutor create_executor)
{
    pool.queue = std::make_shared<background_job_queue>();
    pool.create_thread
        = [create_executor](
              std::shared_ptr<background_job_queue> queue,
              std::shared_ptr<background_thread_data_proxy> data_proxy) {
              return std::thread(background_execution_loop<Executor>(
                  std::move(queue), std::move(data_proxy), create_executor()));
          };
    for (size_t i = 0; i != initial_thread_count; ++i)
        add_background_thread(pool);
}

void
shut_down_pool(background_execution_pool& pool);

bool
is_pool_idle(background_execution_pool& pool);

void
clear_canceled_jobs(background_execution_pool& pool);

// Add a background job to the given execution pool and take care of the
// mechanics for ensuring that a thread gets woken up to handle it (or created,
// depending on the flags).
//
void
queue_background_job(
    background_execution_pool& pool,
    background_job_ptr job_ptr,
    background_job_flag_set flags);

// Add a job for the execution pool to execute.
//
// The returned background_job_controller can be used to monitor the job's
// status and cancel it if needed. The controller does NOT own the job. It can
// safely be discarded if it's not useful. If the job is no longer needed, it
// should be explicitly canceled before discarding the controller.
//
// :job is a pointer to the job object that should be executed. The pool will
// assume owernship of it.
//
// :priority controls the priority of the job. A higher number means higher
// priority. Negative numbers are OK, and 0 is taken to be the default/neutral
// priority.
//
background_job_controller
add_background_job(
    background_execution_pool& pool,
    std::unique_ptr<background_job_interface> job,
    background_job_flag_set flags = NO_FLAGS,
    int priority = 0);

struct basic_executor
{
    void
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        background_job_interface& job)
    {
        job.execute(check_in, reporter);
    }
};

} // namespace detail

// struct background_job_execution_loop
// {
//     background_job_execution_loop(
//         std::shared_ptr<background_job_queue> queue,
//         std::shared_ptr<background_thread_data_proxy> data_proxy)
//         : queue_(std::move(queue)), data_proxy_(std::move(data_proxy))
//     {
//     }
//     void
//     operator()();

//  private:
//     std::shared_ptr<background_job_queue> queue_;
//     std::shared_ptr<background_thread_data_proxy> data_proxy_;
// };

// void
// record_failure(
//     background_job_execution_data& job,
//     char const* msg,
//     bool transient_failure);

} // namespace cradle

#endif
