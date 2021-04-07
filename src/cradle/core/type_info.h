#ifndef CRADLE_CORE_TYPE_INFO_HPP
#define CRADLE_CORE_TYPE_INFO_HPP

#include <vector>

#include <cradle/core/api_types.hpp>

namespace cradle {

// get_type_info<T>() is the external API for getting the CRADLE type info for
// the type T.
template<class T>
api_type_info
get_type_info()
{
    api_type_info info;
    type_info_query<T>::get(&info);
    return info;
}

// get_definitive_type_info<T>() is the external API for getting the definitive
// CRADLE type info for the type T.
template<class T>
api_type_info
get_definitive_type_info()
{
    api_type_info info;
    definitive_type_info_query<T>::get(&info);
    return info;
}

// get_enum_type_info<T>() is the external API for getting the enum type info
// for the type T.
template<class T>
api_enum_info
get_enum_type_info()
{
    api_enum_info info;
    enum_type_info_query<T>::get(&info);
    return info;
}

} // namespace cradle

#endif
