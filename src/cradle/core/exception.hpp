#ifndef CRADLE_CORE_EXCEPTION_HPP
#define CRADLE_CORE_EXCEPTION_HPP

#include <cradle/core/type_definitions.hpp>

#include <boost/exception/all.hpp>
#include <boost/stacktrace.hpp>

namespace cradle {

// The following macros are simple wrappers around Boost.Exception to codify
// how that library should be used within CRADLE.

#define CRADLE_DEFINE_EXCEPTION(id)                                           \
    struct id : virtual boost::exception, virtual std::exception              \
    {                                                                         \
        char const*                                                           \
        what() const noexcept                                                 \
        {                                                                     \
            return boost::diagnostic_information_what(*this);                 \
        }                                                                     \
    };

#define CRADLE_DEFINE_ERROR_INFO(T, id)                                       \
    typedef boost::error_info<struct id##_info_tag, T> id##_info;

CRADLE_DEFINE_ERROR_INFO(boost::stacktrace::stacktrace, stacktrace)

#define CRADLE_THROW(x)                                                       \
    BOOST_THROW_EXCEPTION(                                                    \
        (x) << stacktrace_info(boost::stacktrace::stacktrace()))

using boost::get_error_info;

// get_required_error_info is just like get_error_info except that it requires
// the info to be present and returns a const reference to it. If the info is
// missing, it throws its own exception.
CRADLE_DEFINE_EXCEPTION(missing_error_info)
CRADLE_DEFINE_ERROR_INFO(string, error_info_id)
CRADLE_DEFINE_ERROR_INFO(string, wrapped_exception_diagnostics)
template<class ErrorInfo, class Exception>
typename ErrorInfo::error_info::value_type const&
get_required_error_info(Exception const& e)
{
    typename ErrorInfo::error_info::value_type const* info
        = get_error_info<ErrorInfo>(e);
    if (!info)
    {
        CRADLE_THROW(
            missing_error_info()
            << error_info_id_info(typeid(ErrorInfo).name())
            << wrapped_exception_diagnostics_info(
                   boost::diagnostic_information(e)));
    }
    return *info;
}

} // namespace cradle

#endif
