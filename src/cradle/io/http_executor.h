#ifndef CRADLE_IO_HTTP_EXECUTOR_H
#define CRADLE_IO_HTTP_EXECUTOR_H

#include <cradle/background/execution_pool.h>
#include <cradle/io/http_requests.hpp>

namespace cradle {

struct http_request_job : background_job_interface
{
    http_connection* connection;
};

struct http_request_executor
{
    http_request_executor(http_request_system& system) : connection_(system)
    {
    }

    void
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        background_job_interface& job);

 private:
    http_connection connection_;
};

} // namespace cradle

#endif
