#ifndef CRADLE_THINKNODE_PROVIDER_H
#define CRADLE_THINKNODE_PROVIDER_H

#include <cradle/core.h>
#include <cradle/core/monitoring.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

CRADLE_DEFINE_EXCEPTION(thinknode_provider_error)
// This exception provides internal_error_message_info.

struct provider_app_interface
{
    virtual dynamic
    execute_function(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        std::string const& name,
        dynamic_array const& args) const = 0;
};

// Implement a calculation provider for an API.
void
provide_calculations(
    int argc, char const* const* argv, provider_app_interface const& app);

} // namespace cradle

#endif
