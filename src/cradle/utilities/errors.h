#ifndef CRADLE_UTILITIES_ERRORS_H
#define CRADLE_UTILITIES_ERRORS_H

namespace cradle {

// If an error occurs internally within library that provides its own
// error messages, this is used to convey that message.
CRADLE_DEFINE_ERROR_INFO(string, internal_error_message)

// This can be used to flag errors that represent failed checks on conditions
// that should be guaranteed internally.
CRADLE_DEFINE_EXCEPTION(internal_check_failed)

// This exception is used when a low-level system call fails (one that should
// generally always work) and there is really no point in creating a specific
// exception type for it.
CRADLE_DEFINE_EXCEPTION(system_call_failed)
CRADLE_DEFINE_ERROR_INFO(string, failed_system_call)

}

#endif
