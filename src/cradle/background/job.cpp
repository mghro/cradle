#include <cradle/background/job.h>

#include <cradle/background/internals.h>

namespace cradle {

background_job_controller::~background_job_controller()
{
    this->cancel();
}

void
background_job_controller::reset()
{
    this->cancel();
    job_.reset();
}

background_job_state
background_job_controller::state() const
{
    assert(job_);
    return job_->state;
}

optional<float>
background_job_controller::progress() const
{
    assert(job_);
    return decode_progress(job_->progress.load());
}

void
background_job_controller::cancel()
{
    if (job_)
        job_->cancel = true;
}

} // namespace cradle
