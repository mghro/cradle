#include <cradle/core/dynamic.hpp>

#include <algorithm>

#include <cradle/common.hpp>
#include <cradle/encodings/yaml.hpp>

namespace cradle {

std::ostream&
operator<<(std::ostream& s, value_type t)
{
    switch (t)
    {
        case value_type::NIL:
            s << "nil";
            break;
        case value_type::BOOLEAN:
            s << "boolean";
            break;
        case value_type::INTEGER:
            s << "integer";
            break;
        case value_type::FLOAT:
            s << "float";
            break;
        case value_type::STRING:
            s << "string";
            break;
        case value_type::BLOB:
            s << "blob";
            break;
        case value_type::DATETIME:
            s << "datetime";
            break;
        case value_type::ARRAY:
            s << "array";
            break;
        case value_type::MAP:
            s << "map";
            break;
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("value_type") << enum_value_info(int(t)));
    }
    return s;
}

void
check_type(value_type expected, value_type actual)
{
    if (expected != actual)
    {
        CRADLE_THROW(
            type_mismatch() << expected_value_type_info(expected)
                            << actual_value_type_info(actual));
    }
}

dynamic::dynamic(std::initializer_list<dynamic> list)
{
    // If this is a list of arrays, all of which are length two and have
    // strings as their first elements, treat it as a map.
    if (std::all_of(list.begin(), list.end(), [](dynamic const& v) {
            return v.type() == value_type::ARRAY
                   && cast<dynamic_array>(v).size() == 2
                   && cast<dynamic_array>(v)[0].type() == value_type::STRING;
        }))
    {
        dynamic_map map;
        for (auto v : list)
        {
            auto const& array = cast<dynamic_array>(v);
            map[array[0]] = array[1];
        }
        *this = std::move(map);
    }
    else
    {
        *this = dynamic_array(list);
    }
}

void
dynamic::set(nil_t _)
{
    type_ = value_type::NIL;
}
void
dynamic::set(bool v)
{
    type_ = value_type::BOOLEAN;
    value_ = v;
}
void
dynamic::set(integer v)
{
    type_ = value_type::INTEGER;
    value_ = v;
}
void
dynamic::set(double v)
{
    type_ = value_type::FLOAT;
    value_ = v;
}
void
dynamic::set(string const& v)
{
    type_ = value_type::STRING;
    value_ = v;
}
void
dynamic::set(string&& v)
{
    type_ = value_type::STRING;
    value_ = std::move(v);
}
void
dynamic::set(blob const& v)
{
    type_ = value_type::BLOB;
    value_ = v;
}
void
dynamic::set(blob&& v)
{
    type_ = value_type::BLOB;
    value_ = std::move(v);
}
void
dynamic::set(boost::posix_time::ptime const& v)
{
    type_ = value_type::DATETIME;
    value_ = v;
}
void
dynamic::set(boost::posix_time::ptime&& v)
{
    type_ = value_type::DATETIME;
    value_ = std::move(v);
}
void
dynamic::set(dynamic_array const& v)
{
    type_ = value_type::ARRAY;
    value_ = v;
}
void
dynamic::set(dynamic_array&& v)
{
    type_ = value_type::ARRAY;
    value_ = std::move(v);
}
void
dynamic::set(dynamic_map const& v)
{
    type_ = value_type::MAP;
    value_ = v;
}
void
dynamic::set(dynamic_map&& v)
{
    type_ = value_type::MAP;
    value_ = std::move(v);
}

void
swap(dynamic& a, dynamic& b)
{
    using std::swap;
    swap(a.type_, b.type_);
    swap(a.value_, b.value_);
}

std::ostream&
operator<<(std::ostream& os, dynamic const& v)
{
    os << value_to_diagnostic_yaml(v);
    return os;
}

std::ostream&
operator<<(std::ostream& os, std::list<dynamic> const& v)
{
    os << dynamic(std::vector<dynamic>{std::begin(v), std::end(v)});
    return os;
}

size_t
deep_sizeof(dynamic const& v)
{
    return sizeof(dynamic) + apply_to_dynamic(CRADLE_LAMBDIFY(deep_sizeof), v);
}

size_t
hash_value(dynamic const& x)
{
    return apply_to_dynamic(CRADLE_LAMBDIFY(invoke_hash), x);
}

// COMPARISON OPERATORS

bool
operator==(dynamic const& a, dynamic const& b)
{
    if (a.type() != b.type())
        return false;
    return apply_to_dynamic_pair(
        [](auto const& x, auto const& y) { return x == y; }, a, b);
}
bool
operator!=(dynamic const& a, dynamic const& b)
{
    return !(a == b);
}

bool
operator<(dynamic const& a, dynamic const& b)
{
    if (a.type() != b.type())
        return a.type() < b.type();
    return apply_to_dynamic_pair(
        [](auto const& x, auto const& y) { return x < y; }, a, b);
}
bool
operator<=(dynamic const& a, dynamic const& b)
{
    return !(b < a);
}
bool
operator>(dynamic const& a, dynamic const& b)
{
    return b < a;
}
bool
operator>=(dynamic const& a, dynamic const& b)
{
    return !(a < b);
}

dynamic const&
get_field(dynamic_map const& r, string const& field)
{
    dynamic const* v;
    if (!get_field(&v, r, field))
    {
        CRADLE_THROW(missing_field() << field_name_info(field));
    }
    return *v;
}

dynamic&
get_field(dynamic_map& r, string const& field)
{
    dynamic* v;
    if (!get_field(&v, r, field))
    {
        CRADLE_THROW(missing_field() << field_name_info(field));
    }
    return *v;
}

bool
get_field(dynamic const** v, dynamic_map const& r, string const& field)
{
    auto i = r.find(dynamic(field));
    if (i == r.end())
        return false;
    *v = &i->second;
    return true;
}

bool
get_field(dynamic** v, dynamic_map& r, string const& field)
{
    auto i = r.find(dynamic(field));
    if (i == r.end())
        return false;
    *v = &i->second;
    return true;
}

dynamic const&
get_union_tag(dynamic_map const& map)
{
    if (map.size() != 1)
    {
        CRADLE_THROW(multifield_union());
    }
    return map.begin()->first;
}

void
add_dynamic_path_element(boost::exception& e, dynamic const& path_element)
{
    std::list<dynamic>* info = get_error_info<dynamic_value_path_info>(e);
    if (info)
    {
        info->push_front(path_element);
    }
    else
    {
        e << dynamic_value_path_info(std::list<dynamic>({path_element}));
    }
}

bool
value_requires_coercion(
    std::function<api_type_info(api_named_type_reference const& ref)> const&
        look_up_named_type,
    api_type_info const& type,
    dynamic const& value)
{
    auto recurse =
        [&look_up_named_type](api_type_info const& type, dynamic const& value) {
            return value_requires_coercion(look_up_named_type, type, value);
        };

    switch (get_tag(type))
    {
        case api_type_info_tag::ARRAY: {
            integer index = 0;
            for (auto const& item : cast<dynamic_array>(value))
            {
                auto this_index = index++;
                try
                {
                    if (recurse(as_array(type).element_schema, item))
                        return true;
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, this_index);
                    throw;
                }
            }
            return false;
        }
        case api_type_info_tag::BLOB:
            check_type(value_type::BLOB, value.type());
            return false;
        case api_type_info_tag::BOOLEAN:
            check_type(value_type::BOOLEAN, value.type());
            return false;
        case api_type_info_tag::DATETIME:
            // Be forgiving of clients that leave their datetimes as strings.
            if (value.type() == value_type::STRING)
            {
                try
                {
                    parse_ptime(cast<string>(value));
                    return true;
                }
                catch (...)
                {
                }
            }
            check_type(value_type::DATETIME, value.type());
            return false;
        case api_type_info_tag::DYNAMIC:
            return false;
        case api_type_info_tag::ENUM:
            check_type(value_type::STRING, value.type());
            if (as_enum(type).find(cast<string>(value)) == as_enum(type).end())
            {
                CRADLE_THROW(
                    cradle::invalid_enum_string()
                    << cradle::enum_string_info(cast<string>(value)));
            }
            return false;
        case api_type_info_tag::FLOAT:
            if (value.type() == value_type::INTEGER)
                return true;
            check_type(value_type::FLOAT, value.type());
            return false;
        case api_type_info_tag::INTEGER:
            if (value.type() == value_type::FLOAT)
            {
                double d = cast<double>(value);
                integer i = boost::numeric_cast<integer>(d);
                // Check that coercion doesn't change the value.
                if (boost::numeric_cast<double>(i) == d)
                    return true;
            }
            check_type(value_type::INTEGER, value.type());
            return false;
        case api_type_info_tag::MAP: {
            auto const& map_type = as_map(type);
            // This is a little hack to support the fact that JSON maps are
            // encoded as arrays and they don't get recognized as maps when
            // they're empty.
            if (value.type() == value_type::ARRAY
                && cast<dynamic_array>(value).empty())
            {
                return true;
            }
            for (auto const& key_value : cast<dynamic_map>(value))
            {
                try
                {
                    if (recurse(map_type.key_schema, key_value.first)
                        || recurse(map_type.key_schema, key_value.second))
                    {
                        return true;
                    }
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, key_value.first);
                    throw;
                }
            }
            return false;
        }
        case api_type_info_tag::NAMED:
            return recurse(look_up_named_type(as_named(type)), value);
        case api_type_info_tag::NIL:
        default:
            check_type(value_type::NIL, value.type());
            return false;
        case api_type_info_tag::OPTIONAL_: {
            auto const& map = cast<dynamic_map>(value);
            auto const& tag = cast<string>(cradle::get_union_tag(map));
            if (tag == "some")
            {
                try
                {
                    return recurse(as_optional_(type), get_field(map, "some"));
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, "some");
                    throw;
                }
            }
            else if (tag == "none")
            {
                check_type(value_type::NIL, get_field(map, "none").type());
                return false;
            }
            else
            {
                CRADLE_THROW(
                    invalid_optional_type() << optional_type_tag_info(tag));
            }
        }
        case api_type_info_tag::REFERENCE:
            check_type(value_type::STRING, value.type());
            return false;
        case api_type_info_tag::STRING:
            check_type(value_type::STRING, value.type());
            return false;
        case api_type_info_tag::STRUCTURE: {
            auto const& structure_type = as_structure(type);
            auto const& map = cast<dynamic_map>(value);
            for (auto const& key_value : structure_type)
            {
                auto const& field_name = key_value.first;
                auto const& field_info = key_value.second;
                dynamic const* field_value;
                bool field_present
                    = get_field(&field_value, map, key_value.first);
                if (field_present)
                {
                    try
                    {
                        if (recurse(field_info.schema, *field_value))
                            return true;
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, field_name);
                        throw;
                    }
                }
                else if (!field_info.omissible || !*field_info.omissible)
                {
                    CRADLE_THROW(
                        missing_field() << field_name_info(field_name));
                }
            }
            return false;
        }
        case api_type_info_tag::UNION: {
            auto const& union_type = as_union(type);
            auto const& map = cast<dynamic_map>(value);
            auto const& tag = cast<string>(cradle::get_union_tag(map));
            for (auto const& key_value : union_type)
            {
                auto const& member_name = key_value.first;
                auto const& member_info = key_value.second;
                if (tag == member_name)
                {
                    try
                    {
                        return recurse(
                            member_info.schema, get_field(map, member_name));
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, member_name);
                        throw;
                    }
                }
            }
            CRADLE_THROW(
                cradle::invalid_enum_string() <<
                // This should technically include enum_id_info.
                cradle::enum_string_info(tag));
        }
    }
}

void
coerce_value_impl(
    std::function<api_type_info(api_named_type_reference const& ref)> const&
        look_up_named_type,
    api_type_info const& type,
    dynamic& value)
{
    auto recurse
        = [&look_up_named_type](api_type_info const& type, dynamic& value) {
              coerce_value_impl(look_up_named_type, type, value);
          };

    switch (get_tag(type))
    {
        case api_type_info_tag::ARRAY: {
            integer index = 0;
            for (dynamic& item : cast<dynamic_array>(value))
            {
                auto this_index = index++;
                try
                {
                    recurse(as_array(type).element_schema, item);
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, this_index);
                    throw;
                }
            }
            break;
        }
        case api_type_info_tag::BLOB:
            check_type(value_type::BLOB, value.type());
            break;
        case api_type_info_tag::BOOLEAN:
            check_type(value_type::BOOLEAN, value.type());
            break;
        case api_type_info_tag::DATETIME:
            // Be forgiving of clients that leave their datetimes as strings.
            if (value.type() == value_type::STRING)
            {
                try
                {
                    value = dynamic(parse_ptime(cast<string>(value)));
                    break;
                }
                catch (...)
                {
                }
            }
            check_type(value_type::DATETIME, value.type());
            break;
        case api_type_info_tag::DYNAMIC:
            break;
        case api_type_info_tag::ENUM:
            check_type(value_type::STRING, value.type());
            if (as_enum(type).find(cast<string>(value)) == as_enum(type).end())
            {
                CRADLE_THROW(
                    cradle::invalid_enum_string()
                    << cradle::enum_string_info(cast<string>(value)));
            }
            break;
        case api_type_info_tag::FLOAT:
            if (value.type() == value_type::INTEGER)
            {
                value = boost::numeric_cast<double>(cast<integer>(value));
                break;
            }
            check_type(value_type::FLOAT, value.type());
            break;
        case api_type_info_tag::INTEGER:
            if (value.type() == value_type::FLOAT)
            {
                double d = cast<double>(value);
                integer i = boost::numeric_cast<integer>(d);
                // Check that coercion doesn't change the value.
                if (boost::numeric_cast<double>(i) == d)
                {
                    value = dynamic(i);
                    break;
                }
            }
            check_type(value_type::INTEGER, value.type());
            break;
        case api_type_info_tag::MAP: {
            auto const& map_type = as_map(type);
            // This is a little hack to support the fact that JSON maps are
            // encoded as arrays and they don't get recognized as maps when
            // they're empty.
            if (value.type() == value_type::ARRAY
                && cast<dynamic_array>(value).empty())
            {
                value = dynamic_map();
            }
            // Since we can't mutate the keys in the map, first check to see if
            // that's necessary.
            bool key_coercion_required = false;
            for (auto const& key_value : cast<dynamic_map>(value))
            {
                if (value_requires_coercion(
                        look_up_named_type,
                        map_type.key_schema,
                        key_value.first))
                {
                    key_coercion_required = true;
                    break;
                }
            }
            // If the keys need to be coerced, just create a new map.
            if (key_coercion_required)
            {
                dynamic_map coerced;
                for (auto& key_value : cast<dynamic_map>(value))
                {
                    try
                    {
                        dynamic key = key_value.first;
                        dynamic value = std::move(key_value.second);
                        recurse(map_type.key_schema, key);
                        recurse(map_type.value_schema, value);
                        coerced[key] = value;
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, key_value.first);
                        throw;
                    }
                }
                value = coerced;
            }
            // Otherwise, coerce the values within the original map.
            else
            {
                for (auto& key_value : cast<dynamic_map>(value))
                {
                    try
                    {
                        recurse(map_type.value_schema, key_value.second);
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, key_value.first);
                        throw;
                    }
                }
            }
            break;
        }
        case api_type_info_tag::NAMED:
            recurse(look_up_named_type(as_named(type)), value);
            break;
        case api_type_info_tag::NIL:
        default:
            check_type(value_type::NIL, value.type());
            break;
        case api_type_info_tag::OPTIONAL_: {
            auto& map = cast<dynamic_map>(value);
            auto const& tag = cast<string>(cradle::get_union_tag(map));
            if (tag == "some")
            {
                try
                {
                    recurse(as_optional_(type), get_field(map, "some"));
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, "some");
                    throw;
                }
            }
            else if (tag == "none")
            {
                check_type(value_type::NIL, get_field(map, "none").type());
            }
            else
            {
                CRADLE_THROW(
                    invalid_optional_type() << optional_type_tag_info(tag));
            }
            break;
        }
        case api_type_info_tag::REFERENCE:
            check_type(value_type::STRING, value.type());
            break;
        case api_type_info_tag::STRING:
            check_type(value_type::STRING, value.type());
            break;
        case api_type_info_tag::STRUCTURE: {
            auto const& structure_type = as_structure(type);
            auto& map = cast<dynamic_map>(value);
            for (auto const& key_value : structure_type)
            {
                auto const& field_name = key_value.first;
                auto const& field_info = key_value.second;
                dynamic* field_value;
                bool field_present
                    = get_field(&field_value, map, key_value.first);
                if (field_present)
                {
                    try
                    {
                        recurse(field_info.schema, *field_value);
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, field_name);
                        throw;
                    }
                }
                else if (!field_info.omissible || !*field_info.omissible)
                {
                    CRADLE_THROW(
                        missing_field() << field_name_info(field_name));
                }
            }
            break;
        }
        case api_type_info_tag::UNION: {
            auto const& union_type = as_union(type);
            auto& map = cast<dynamic_map>(value);
            auto const& tag = cast<string>(cradle::get_union_tag(map));
            for (auto const& key_value : union_type)
            {
                auto const& member_name = key_value.first;
                auto const& member_info = key_value.second;
                if (tag == member_name)
                {
                    try
                    {
                        recurse(
                            member_info.schema, get_field(map, member_name));
                        return;
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, member_name);
                        throw;
                    }
                }
            }
            CRADLE_THROW(
                cradle::invalid_enum_string() <<
                // This should technically include enum_id_info.
                cradle::enum_string_info(tag));
        }
    }
}

dynamic
coerce_value(
    std::function<api_type_info(api_named_type_reference const& ref)> const&
        look_up_named_type,
    api_type_info const& type,
    dynamic value)
{
    coerce_value_impl(look_up_named_type, type, value);
    return value;
}

} // namespace cradle
