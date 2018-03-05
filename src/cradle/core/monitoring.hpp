#ifndef CRADLE_CORE_MONITORING_HPP
#define CRADLE_CORE_MONITORING_HPP

namespace cradle {

// A lot of CRADLE algorithms use callbacks to report progress or check in with
// their callers (which can, for example, pause or terminate the algorithm).
// The following are the interface definitions for these callbacks and some
// utilities for constructing them.

// A progress reporter object gets called periodically with the progress of the
// algorithm (0 is just started, 1 is done).
struct progress_reporter_interface
{
    virtual void
    operator()(float)
        = 0;
};
// If you don't want to know about the progress, pass one of these.
struct null_progress_reporter : progress_reporter_interface
{
    void
    operator()(float)
    {
    }
};

// When an algorithm is divided into subtasks, this can be used to translate
// the progress of the subtasks into the overall progress of the main task.
// When creating the progress reporter for each subtask, you specify the
// portion of the overall job that the subtask represents. The state variable
// must be shared across all subtasks and tracks the overall progress of the
// main task. It's up to the caller to ensure that the sum of the portions of
// all subtasks is 1.
struct task_subdivider_state
{
    float offset;
    task_subdivider_state() : offset(0)
    {
    }
};
struct subtask_progress_reporter : progress_reporter_interface
{
    subtask_progress_reporter(
        progress_reporter_interface& parent_reporter,
        task_subdivider_state& state,
        float portion)
        : parent_reporter_(&parent_reporter), state_(state), portion_(portion)
    {
        offset_ = state_.offset;
        state_.offset += portion;
    }

    void
    operator()(float progress)
    {
        (*parent_reporter_)(offset_ + progress * portion_);
    }

 private:
    progress_reporter_interface* parent_reporter_;
    task_subdivider_state& state_;
    float offset_;
    float portion_;
};

// When an algorithm is divided into subtasks, and called from within a loop
// this sub_progress_reporter can be used.
// Takes in the parent reporter which it will call to report progress
// Takes in an offset which is the current progress when then reporter is made
// Takes in a scale which is used when this reporter is created in a loop
struct sub_progress_reporter : progress_reporter_interface
{
    sub_progress_reporter(
        progress_reporter_interface& parent_reporter, float offset, float scale)
        : parent_reporter_(&parent_reporter), offset_(offset), scale_(scale)
    {
    }

    void
    operator()(float progress)
    {
        (*parent_reporter_)(offset_ + progress * scale_);
    }

 private:
    progress_reporter_interface* parent_reporter_;
    float offset_;
    float scale_;
};

// Algorithms call this to check in with the caller every few milliseconds.
// This can be used to abort the algorithm by throwing an exception.
struct check_in_interface
{
    virtual void
    operator()()
        = 0;
};
// If you don't need the algorithm to check in, pass one of these.
struct null_check_in : check_in_interface
{
    void
    operator()()
    {
    }
};

// If you need an algorithm to check in with two different controllers, you
// can use this to merge the check_in objects supplied by the two controllers.
struct merged_check_in : check_in_interface
{
    merged_check_in(check_in_interface* a, check_in_interface* b) : a(a), b(b)
    {
    }

    void
    operator()()
    {
        (*a)();
        (*b)();
    }

 private:
    check_in_interface* a;
    check_in_interface* b;
};

} // namespace cradle

#endif
