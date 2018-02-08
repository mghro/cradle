#include "utilities.hpp"

using namespace cradle;

cradle::http_response
make_mock_response(cradle::string const& body)
{
    http_response mock_response;
    mock_response.status_code = 200;
    mock_response.body = make_string_blob(body);
    return mock_response;
}
