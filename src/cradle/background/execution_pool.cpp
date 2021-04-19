#include <cradle/background/execution_pool.h>

#include <boost/algorithm/string.hpp>

namespace cradle {

// void
// record_failure(
//     background_job_queue& queue,
//     background_job_ptr& job,
//     string msg,
//     bool is_transient)
// {
//     // job->state = background_job_state::FAILED;
//     // boost::lock_guard<boost::mutex> lock(queue.mutex);
//     // inc_version(queue.version);
//     // background_job_failure failure;
//     // failure.is_transient = is_transient;
//     // failure.message = msg; // != '\0' ? msg : "unknown error";
//     // failure.job = job;
//     // queue.failed_jobs.push_back(failure);
// }

namespace detail {

// void web_request_processing_loop::operator()()
// {
//     while (1)
//     {
//         auto& queue = *queue_;

//         // Wait until the queue has a job in it, and then grab the job.
//         background_job_ptr job;
//         {
//             boost::unique_lock<boost::mutex> lock(queue.mutex);
//             inc_version(queue.version);
//             ++queue.n_idle_threads;
//             while (queue.jobs.empty())
//                 queue.cv.wait(lock);
//             job = queue.jobs.top();
//             inc_version(queue.version);
//             queue.jobs.pop();
//             if (!job->hidden)
//                 --queue.reported_size;
//             --queue.n_idle_threads;

//             // If it's already been instructed to cancel, cancel it.
//             if (job->cancel)
//             {
//                 job->state = background_job_state::CANCELED;
//                 queue.job_info.erase(&*job);
//                 continue;
//             }
//         }

//         // If its inputs aren't ready, put it in the waiting queue.
//         if (!job->job->inputs_ready())
//         {
//             boost::lock_guard<boost::mutex> lock(queue.mutex);
//             inc_version(queue.version);
//             queue.waiting_jobs.push(job);
//             if (!job->hidden)
//                 ++queue.reported_size;
//         }
//         // Otherwise, execute it.
//         else
//         {
//             {
//                 boost::lock_guard<boost::mutex> lock(data_proxy_->mutex);
//                 data_proxy_->active_job = job;
//             }

//             assert(dynamic_cast<background_web_job*>(job->job));
//             auto web_job = static_cast<background_web_job*>(job->job);
//             assert(web_job->system);

//             try
//             {
//                 job->state = background_job_state::RUNNING;
//                 background_job_check_in check_in(job);
//                 background_job_progress_reporter reporter(job);
//                 web_job->connection = connection_.get();
//                 job->job->execute(check_in, reporter);
//                 job->state = background_job_state::FINISHED;

//                 // The job is done, so clear out its reference to the
//                 // background execution system.
//                 // Otherwise we'll end up with circular references.
//                 web_job->system.reset();
//             }
//             catch (background_job_canceled&)
//             {
//             }
//             catch (web_request_failure& failure)
//             {
//                 switch (failure.response_code())
//                 {
//                  case 401:
//                     invalidate_authentication_data(*web_job->system,
//                         background_authentication_state::NO_CREDENTIALS);
//                     break;
//                  case 483:
//                     invalidate_authentication_data(*web_job->system,
//                         background_authentication_state::SESSION_EXPIRED);
//                     break;
//                  case 484:
//                     invalidate_authentication_data(*web_job->system,
//                         background_authentication_state::SESSION_TIMED_OUT);
//                     break;
//                  case 481: // missing cookies
//                  case 482: // invalid cookies
//                     // These should never happen unless there's a bug.
//                     assert(0);
//                     // Fall through just in case.
//                  default:
//                     // Record if the failure was transient or a 5XX error
//                     code
//                     // so that these calculations can be retried.
//                     record_failure(queue, job, failure.what(),
//                         failure.is_transient() ||
//                         (failure.response_code() / 100) == 5);
//                     break;
//                 }
//             }
//             catch (cradle::exception& e)
//             {
//                 string msg = string(e.what()) + string("\n\ndebug
//                 details:\n") + "(wrc) " + job->job->get_info().description;
//                 record_failure(queue, job,  msg, e.is_transient());
//             }
//             catch (std::bad_alloc&)
//             {
//                 string msg = "(wrc) " + job->job->get_info().description +
//                 string("\n out of memory"); record_failure(queue, job, msg,
//                 true);
//             }
//             catch (std::exception& e)
//             {
//                 string msg = string(e.what()) + string("\n\ndebug
//                 details:\n") + "(wrs) " + job->job->get_info().description;
//                 record_failure(queue, job, msg, false);
//             }
//             catch (...)
//             {
//                 string msg = "(wrc) " + job->job->get_info().description;
//                 record_failure(queue, job, msg, false);
//             }

//             {
//                 boost::unique_lock<boost::mutex> lock(queue.mutex);
//                 queue.job_info.erase(&*job);
//                 inc_version(queue.version);
//             }

//             {
//                 boost::lock_guard<boost::mutex> lock(data_proxy_->mutex);
//                 data_proxy_->active_job.reset();
//             }
//         }
//     }
// }

// void
// update_status(keyed_data<background_execution_pool_status>& status,
//     background_execution_pool& pool)
// {
//     background_job_queue& queue = *pool.queue;
//     boost::mutex::scoped_lock lock(queue.mutex);
//     auto thread_count = pool.threads.size();
//     refresh_keyed_data(status,
//         combine_ids(get_id(queue.version), make_id(thread_count)));
//     if (!is_valid(status))
//     {
//         background_execution_pool_status new_status;
//         new_status.thread_count = thread_count;
//         for (auto const& f : queue.failed_jobs)
//         {
//             background_job_failure_report report;
//             report.job = f.job.get();
//             report.message = f.message;
//             new_status.transient_failures.push_back(report);
//         }
//         new_status.queued_job_count = queue.reported_size;
//         new_status.idle_thread_count = queue.n_idle_threads;
//         new_status.job_info = queue.job_info;
//         set(status, new_status);
//     }
// }

// void update_status(background_execution_system_status& status,
//     background_execution_system& system)
// {
//     for (unsigned i = 0; i != unsigned(background_job_queue_type::COUNT);
//     ++i)
//         update_status(status.pools[i], system.impl_->pools[i]);
// }

// void static
// get_permanent_failures(std::list<background_job_failure_report>& failures,
//     background_execution_pool& pool)
// {
//     background_job_queue& queue = *pool.queue;
//     boost::mutex::scoped_lock lock(queue.mutex);
//     for (auto i = queue.failed_jobs.begin(); i != queue.failed_jobs.end(); )
//     {
//         if (!i->is_transient)
//         {
//             // Record it.
//             background_job_failure_report report;
//             report.job = i->job.get();
//             report.message = i->message;
//             failures.push_back(report);
//             // Erase it.
//             i = queue.failed_jobs.erase(i);
//         }
//         else
//             ++i;
//     }
// }

// std::list<background_job_failure_report>
// get_permanent_failures(background_execution_system& system)
// {
//     std::list<background_job_failure_report> failures;
//     for (unsigned i = 0; i != unsigned(background_job_queue_type::COUNT);
//     ++i)
//         get_permanent_failures(failures, system.impl_->pools[i]);
//     return failures;
// }

size_t
canceled_job_count(background_job_queue& queue)
{
    std::scoped_lock<std::mutex> lock(queue.mutex);
    job_priority_queue copy = queue.jobs;
    size_t count = 0;
    while (!copy.empty())
    {
        auto top = copy.top();
        if (top->cancel)
            ++count;
        copy.pop();
    }
    return count;
}

void
clear_pending_jobs(background_execution_pool& pool)
{
    auto& queue = *pool.queue;
    std::scoped_lock<std::mutex> lock(queue.mutex);
    ++queue.version;
    queue.jobs = job_priority_queue();
}

void
clear_all_jobs(background_execution_pool& pool)
{
    clear_pending_jobs(pool);

    for (auto& i : pool.threads)
    {
        auto& active_job = i->data_proxy->active_job;
        if (active_job)
            active_job->cancel = true;
    }

    {
        auto& queue = *pool.queue;
        std::scoped_lock<std::mutex> lock(queue.mutex);
        ++queue.version;
        queue.failed_jobs.clear();
    }
}

void
clear_canceled_jobs(background_job_queue& queue, job_priority_queue& pqueue)
{
    job_priority_queue filtered;
    while (!pqueue.empty())
    {
        auto job = pqueue.top();
        if (job->cancel)
        {
            if (!(job->flags & BACKGROUND_JOB_HIDDEN))
                --queue.reported_size;
            queue.job_info.erase(&*job);
        }
        else
            filtered.push(job);
        pqueue.pop();
    }
    pqueue = filtered;
}

void
clear_canceled_jobs(background_execution_pool& pool)
{
    auto& queue = *pool.queue;
    std::scoped_lock<std::mutex> lock(queue.mutex);
    clear_canceled_jobs(queue, queue.jobs);
}

void
shut_down_pool(background_execution_pool& pool)
{
    clear_all_jobs(pool);
    auto& queue = *pool.queue;
    {
        std::scoped_lock<std::mutex> lock(queue.mutex);
        queue.terminating = true;
    }
    queue.cv.notify_all();
    for (auto& thread : pool.threads)
        thread->thread.join();
}

bool
is_pool_idle(background_execution_pool& pool)
{
    background_job_queue& queue = *pool.queue;
    std::scoped_lock<std::mutex> lock(queue.mutex);
    return queue.n_idle_threads == pool.threads.size() && queue.jobs.empty();
}

void
add_background_thread(background_execution_pool& pool)
{
    auto data_proxy = std::make_shared<background_thread_data_proxy>();
    auto thread = std::make_shared<background_execution_thread>(
        pool.create_thread(pool.queue, data_proxy), data_proxy);
    pool.threads.push_back(thread);
    // lower_thread_priority(thread->thread);
}

void
queue_background_job(
    background_execution_pool& pool,
    background_job_ptr job_ptr,
    background_job_flag_set flags)
{
    background_job_queue& queue = *pool.queue;
    {
        std::scoped_lock<std::mutex> lock(queue.mutex);
        ++queue.version;
        if (!(flags & BACKGROUND_JOB_HIDDEN))
        {
            queue.job_info[&*job_ptr]
                = background_job_info(); // TODO: job_ptr->job->get_info();
            ++queue.reported_size;
        }
        queue.jobs.push(job_ptr);
        // If requested, ensure that there will be an idle thread to pick
        // up the new job.
        if ((flags & BACKGROUND_JOB_SKIP_QUEUE)
            && queue.n_idle_threads < queue.jobs.size())
        {
            add_background_thread(pool);
        }
    }
    queue.cv.notify_one();
}

background_job_controller
add_background_job(
    background_execution_pool& pool,
    std::unique_ptr<background_job_interface> job,
    background_job_flag_set flags,
    int priority)
{
    auto ptr = std::make_shared<detail::background_job_execution_data>(
        std::move(job), flags, priority);
    queue_background_job(pool, ptr, flags);
    return background_job_controller(ptr);
}

} // namespace detail

} // namespace cradle
