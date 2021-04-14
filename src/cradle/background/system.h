#ifndef CRADLE_BACKGROUND_SYSTEM_H
#define CRADLE_BACKGROUND_SYSTEM_H

#include <cradle/background/job.h>
#include <cradle/core.h>
#include <cradle/io/http_requests.hpp>

#include <list>

// A background_execution_system is a flexible means of executing jobs in
// background threads.
//
// This is essentially the same system that exists in Astroid, and it will
// likely be replaced by standard C++ mechanisms once those are fleshed out.
//
// It supports three different types of jobs: calculations, web queries,
// and disk jobs.
//
// For calculations, it maintains a pool of worker threads (one for each
// processor core in the system). Individual jobs are assumed to be
// single-threaded, so each worker thread simply grabs jobs off the queue and
// executes them one at a time.
//
// For web queries, it's assumed that more concurrency is always better, so
// the system allocates threads as needed to ensure that all pending queries
// can execute immediately.
//
// A small, fixed number of threads service disk jobs, as it's assumed that
// they'll mostly be contending for the same resource.

// This file provides the interface for creating and managing a
// background_system as a whole. The API for data retrieval and job creation
// can be found in api.hpp.

namespace cradle {

namespace detail {

struct background_execution_system;

}

struct background_execution_system : noncopyable
{
    background_execution_system();
    ~background_execution_system();

    std::unique_ptr<detail::background_execution_system> impl_;
};

struct background_job_execution_data;

// Clear all the jobs in the system, including those that are currently
// executing.
void
clear_all_jobs(background_execution_system& system);

// Clears out any jobs in the system that have been canceled.
void
clear_canceled_jobs(background_execution_system& system);

// struct background_job_failure_report
// {
//     // the job that failed
//     background_job_execution_data* job;
//     // the associated error message
//     string message;
// };

// struct background_execution_pool_status
// {
//     size_t queued_job_count, thread_count, idle_thread_count;
//     std::list<background_job_failure_report> transient_failures;
//     std::map<background_job_execution_data*, background_job_info> job_info;
// };

// inline size_t
// get_active_thread_count(background_execution_pool_status const& status)
// {
//     return status.thread_count - status.idle_thread_count;
// }

// inline size_t
// get_total_job_count(background_execution_pool_status const& status)
// {
//     return get_active_thread_count(status) + status.queued_job_count
//            + status.transient_failures.size();
// }

// struct background_execution_system_status
// {
//     background_execution_pool_status
//         pools[size_t(background_job_queue_type::COUNT)];
// };

// // Update a view of the status of a background execution system.
// void
// update_status(
//     background_execution_system_status& status,
//     background_execution_system& system);

// // Get a list of jobs that have failed permanently since the last check.
// // (This also clears the system's internal list.)
// std::list<background_job_failure_report>
// get_permanent_failures(background_execution_system& system);

} // namespace cradle

#endif