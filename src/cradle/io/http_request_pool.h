#ifndef CRADLE_IO_HTTP_REQUEST_POOL_H
#define CRADLE_IO_HTTP_REQUEST_POOL_H

#include <cradle/background/execution_pool.h>

namespace cradle {

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

}

#endif
