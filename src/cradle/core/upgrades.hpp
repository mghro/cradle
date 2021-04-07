#ifndef CRADLE_CORE_UPGRADES_HPP
#define CRADLE_CORE_UPGRADES_HPP

#include <cradle/core/type_definitions.h>

namespace cradle {

// Denotes the different kind of upgrades that are available.
api(enum)
enum class upgrade_type
{
    // no upgrade
    NONE,

    /// an empty mutation upgrade (equivalent to a cast)
    /// EMPTY,

    // an upgrade via a custom function
    FUNCTION
};

// static upgrade_type
// merged_upgrade_type(upgrade_type a, upgrade_type b)
// {
//     return a > b ? a : b;
// }

// // Gets the explicit upgrade type for values. This is the default
// // implementation of this and it's expected to be overridden by the user for
// // any type they are defining an upgrade for.
// template<class T>
// upgrade_type
// get_explicit_upgrade_type(T const&)
// {
//     return upgrade_type::NONE;
// }

// #define upgrade_struct(T)
//     static upgrade_type get_explicit_upgrade_type(T const&)
//     {
//         return upgrade_type::FUNCTION;
//     }

// // Gets the upgrade type for values. This is the default implementation
// // of this and it's expected to be overridden.
// template<class T>
// upgrade_type
// get_upgrade_type(T const&, std::vector<std::type_index> parsed_types)
// {
//     return upgrade_type::NONE;
// }

// // Gets the upgrade type for optional values.
// template<class T>
// upgrade_type
// get_upgrade_type(optional<T> const&, std::vector<std::type_index>
// parsed_types)
// {
//     return get_upgrade_type(T(), parsed_types);
// }

// // Gets the upgrade type for object references.
// template<class T>
// upgrade_type
// get_upgrade_type(
//     object_reference<T> const&, std::vector<std::type_index> parsed_types)
// {
//     // object_references are not upgraded. They will be upgraded when you
//     // attempt to get the data associated with the reference.
//     return upgrade_type::NONE;
// }

// // Gets the upgrade type for values stored in a vector.
// template<class T>
// upgrade_type
// get_upgrade_type(
//     std::vector<T> const&, std::vector<std::type_index> parsed_types)
// {
//     return get_upgrade_type(T(), parsed_types);
// }

// // Gets the upgrade type for values stored in a map
// template<class Key, class Value>
// upgrade_type
// get_upgrade_type(
//     std::map<Key, Value> const&, std::vector<std::type_index> parsed_types)
// {
//     return merged_upgrade_type(
//         get_upgrade_type(Key(), parsed_types),
//         get_upgrade_type(Value(), parsed_types));
// }

// // Gets the upgrade type for values stored in an array.
// template<class T>
// upgrade_type
// get_upgrade_type(array<T> const&, std::vector<std::type_index> parsed_types)
// {
//     return get_upgrade_type(T(), parsed_types);
// }

// // Checks if dynamic_map has field and calls upgrade value if it does.
// template<class T>
// void
// upgrade_field(T* x, dynamic_map const& r, string const& field)
// {
//     auto i = r.find(value(field));
//     if (i != r.end())
//     {
//         upgrade_value(x, i->second);
//     }
// }

// // Creates a dynamic_map and attempt and attempts to upgrade fields.
// template<class T>
// void
// upgrade_field(T* x, dynamic const& v, string const& field)
// {
//     auto const& r = cradle::cast<cradle::dynamic_map>(v);
//     upgrade_field(x, r, field);
// }

// // Handles upgrading values that need to be put in a container.
// template<class T>
// T
// auto_upgrade_value_for_container(dynamic const& v)
// {
//     T x;
//     upgrade_value(&x, v);
//     return x;
// }

// // Handles updating values stored in a vector.
// template<class T>
// void
// auto_upgrade_value(std::vector<T>* x, dynamic const& v)
// {
//     cradle::value_list const& l = cradle::cast<cradle::value_list>(v);

//     for (auto const& i : l)
//     {
//         x->push_back(auto_upgrade_value_for_container<T>(i));
//     }
// }

// // Handles updating values stored in an array.
// template<class T>
// void
// auto_upgrade_value(array<T>* x, dynamic const& v)
// {
//     // TODO: figure out how to upgrade array values, you don't know size
//     that
//     // previous structure was because it could have changed (props added or
//     // removed)

//     // only allow regular types in arrays
//     from_value(x, v);
// }

// // Handles updating values stored in a map.
// template<class Key, class Value>
// void
// auto_upgrade_value(std::map<Key, Value>* x, dynamic const& v)
// {
//     cradle::dynamic_map const& l = cradle::cast<cradle::value_map>(v);
//     for (auto const& i : l)
//     {
//         x->insert(std::pair<Key, Value>(
//             auto_upgrade_value_for_container<Key>(i.first),
//             auto_upgrade_value_for_container<Value>(i.second)));
//     }
// }

// // Handles updating values for object_reference types.
// // Object_reference types do not have upgrades so its just calling
// from_value. template<class T> void auto_upgrade_value(object_reference<T>*
// x, dynamic const& v)
// {
//     from_value(x, v);
// }

// // Handles updating values for basic types.
// // Basic types do not have upgrades so its just calling from_value.
// template<class T>
// void
// auto_upgrade_value(T* x, dynamic const& v)
// {
//     from_value(x, v);
// }

// // Generic upgrade_value function.
// template<class T>
// void
// upgrade_value(T* x, dynamic const& v)
// {
//     auto_upgrade_value(x, v);
// }

} // namespace cradle

#endif
