#ifndef CRADLE_TESTS_THINKNODE_UTILITIES_HPP
#define CRADLE_TESTS_THINKNODE_UTILITIES_HPP

#include <cradle/io/http_types.hpp>

cradle::http_response
make_mock_response(cradle::string const& body);

#endif
