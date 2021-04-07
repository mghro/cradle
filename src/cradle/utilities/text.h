#ifndef CRADLE_UTILITIES_TEXT_H
#define CRADLE_UTILITIES_TEXT_H

#include <boost/lexical_cast.hpp>

namespace cradle {

using boost::lexical_cast;

// If a simple parsing operation fails, this exception can be thrown.
CRADLE_DEFINE_EXCEPTION(parsing_error)
CRADLE_DEFINE_ERROR_INFO(string, expected_format)
CRADLE_DEFINE_ERROR_INFO(string, parsed_text)
CRADLE_DEFINE_ERROR_INFO(string, parsing_error)

} // namespace cradle

#endif
