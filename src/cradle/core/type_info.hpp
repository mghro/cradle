#ifndef CRADLE_CORE_TYPE_INFO_HPP
#define CRADLE_CORE_TYPE_INFO_HPP

#include <vector>

#include <cradle/core/api_types.hpp>
#include <cradle/core/utilities.hpp>

namespace cradle {

// get_type_info<T>() is the external API for getting the CRADLE type info for
// the type T.
template<class T>
auto
get_type_info()
{
    api_type_info info;
    type_info_query<T>::get(&info);
    return info;
}

} // namespace cradle

#endif
