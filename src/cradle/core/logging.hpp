#ifndef CRADLE_CORE_LOGGING_HPP
#define CRADLE_CORE_LOGGING_HPP

#include <sstream>
#include <type_traits>

#include <spdlog/spdlog.h>

#include <cradle/core/dynamic.hpp>

namespace cradle {

namespace detail {

template<class Value>
struct arg_logger
{
    arg_logger(char const* name, Value const& value) : name(name), value(value)
    {
    }

    char const* name;
    Value const& value;
};

template<class Value>
std::ostream&
operator<<(std::ostream& stream, arg_logger<Value> arg)
{
    stream << "\n" << dynamic({{arg.name, to_dynamic(arg.value)}});
    return stream;
}

} // namespace detail

// Create a logger for a function call.
#define CRADLE_LOG_CALL(args)                                                  \
    {                                                                          \
        auto logger = spdlog::get("cradle");                                   \
        std::ostringstream stream;                                             \
        stream << __func__ args;                                               \
        logger->info(stream.str());                                            \
    }

// Log an argument to a function call.
#define CRADLE_LOG_ARG(arg)                                                    \
    cradle::detail::arg_logger<                                                \
        std::remove_reference<std::remove_const<decltype(arg)>::type>::type>(  \
        #arg, arg)

} // namespace cradle

#endif
