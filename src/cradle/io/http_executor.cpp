#include <cradle/io/http_executor.h>

#include <cradle/utilities/errors.h>

namespace cradle {

void
http_request_executor::execute(
    check_in_interface& check_in,
    progress_reporter_interface& reporter,
    background_job_interface& job_object)
{
    http_request_job* job = dynamic_cast<http_request_job*>(&job_object);
    if (!job)
    {
        CRADLE_THROW(
            internal_check_failed() << internal_error_message_info(
                "non-HTTP job scheduled on HTTP executor"));
    }

    job->connection = &this->connection_;
    job->execute(check_in, reporter);
}

} // namespace cradle
