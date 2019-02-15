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

bool
get_field(dynamic const** v, dynamic_map const& r, string const& field)
{
    auto i = r.find(dynamic(field));
    if (i == r.end())
        return false;
    *v = &i->second;
    return true;
}

dynamic const&
get_union_value_type(dynamic_map const& map)
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

template<class Dynamic>
dynamic
coerce_value_impl(
    std::function<api_type_info(api_named_type_reference const& ref)> const&
        look_up_named_type,
    api_type_info const& type,
    Dynamic&& value)
{
    auto recurse
        = [&look_up_named_type](api_type_info const& type, auto&& value) {
              return coerce_value_impl(look_up_named_type, type, value);
          };

    switch (get_tag(type))
    {
        case api_type_info_tag::ARRAY:
            return map(
                [&](auto&& item) {
                    return recurse(
                        as_array(type).element_schema,
                        std::forward<decltype(item)>(item));
                },
                cast<dynamic_array>(std::forward<Dynamic>(value)));
        case api_type_info_tag::BLOB:
            check_type(value.type(), value_type::BLOB);
            return std::forward<Dynamic>(value);
        case api_type_info_tag::BOOLEAN:
            check_type(value.type(), value_type::BOOLEAN);
            return std::forward<Dynamic>(value);
        case api_type_info_tag::DATETIME:
            check_type(value.type(), value_type::DATETIME);
            return std::forward<Dynamic>(value);
        case api_type_info_tag::DYNAMIC:
            return std::forward<Dynamic>(value);
        case api_type_info_tag::ENUM:
            check_type(value.type(), value_type::STRING);
            if (as_enum(type).find(cast<string>(value)) == as_enum(type).end())
            {
                CRADLE_THROW(
                    cradle::invalid_enum_string()
                    << cradle::enum_string_info(cast<string>(value)));
            }
            return std::forward<Dynamic>(value);
        case api_type_info_tag::FLOAT:
            if (value.type() == value_type::INTEGER)
                return dynamic(
                    boost::numeric_cast<double>(cast<integer>(value)));
            check_type(value.type(), value_type::FLOAT);
            return std::forward<Dynamic>(value);
        case api_type_info_tag::INTEGER:
            if (value.type() == value_type::FLOAT)
            {
                double d = cast<double>(value);
                integer i = boost::numeric_cast<integer>(d);
                // Check that coercion doesn't change the value.
                if (boost::numeric_cast<double>(i) == d)
                    return dynamic(i);
            }
            check_type(value.type(), value_type::INTEGER);
            return std::forward<Dynamic>(value);
        case api_type_info_tag::MAP:
        {
            auto const& map_type = as_map(type);
            // This doesn't forward perfectly.
            dynamic_map coerced;
            for (auto const& pair : cast<dynamic_map>(value))
            {
                coerced[recurse(map_type.key_schema, pair.first)]
                    = recurse(map_type.value_schema, pair.second);
            }
            return coerced;
        }
        case api_type_info_tag::NAMED:
            return recurse(look_up_named_type(as_named(type)), value);
        case api_type_info_tag::NIL:
        default:
            check_type(value.type(), value_type::NIL);
            return std::forward<Dynamic>(value);
        case api_type_info_tag::OPTIONAL:
        {
            // This doesn't forward perfectly.
            dynamic_map coerced;
            auto const& map = cast<dynamic_map>(value);
            string tag;
            from_dynamic(&tag, cradle::get_union_value_type(map));
            if (tag == "some")
            {
                try
                {
                    coerced["some"]
                        = recurse(as_optional(type), get_field(map, "some"));
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, "some");
                    throw;
                }
            }
            else if (tag == "none")
            {
                coerced["none"] = nil;
            }
            else
            {
                CRADLE_THROW(
                    invalid_optional_type() << optional_type_tag_info(tag));
            }
            return coerced;
        }
        case api_type_info_tag::REFERENCE:
            check_type(value.type(), value_type::STRING);
            return std::forward<Dynamic>(value);
        case api_type_info_tag::STRING:
            check_type(value.type(), value_type::STRING);
            return std::forward<Dynamic>(value);
        case api_type_info_tag::STRUCTURE:
        {
            auto const& structure_type = as_structure(type);
            // This doesn't forward perfectly.
            dynamic_map coerced;
            auto const& map = cast<dynamic_map>(value);
            for (auto const& pair : structure_type)
            {
                auto const& field_name = pair.first;
                auto const& field_info = pair.second;
                dynamic const* field_value;
                bool field_present = get_field(&field_value, map, pair.first);
                if (field_present)
                {
                    try
                    {
                        coerced[field_name]
                            = recurse(field_info.schema, *field_value);
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
            return coerced;
        }
        case api_type_info_tag::UNION:
        {
            auto const& union_type = as_union(type);
            // This doesn't forward perfectly.
            auto const& map = cast<dynamic_map>(value);
            string tag;
            from_dynamic(&tag, cradle::get_union_value_type(map));
            for (auto const& pair : union_type)
            {
                auto const& member_name = pair.first;
                auto const& member_info = pair.second;
                if (tag == member_name)
                {
                    dynamic_map coerced;
                    coerced[member_name] = recurse(
                        member_info.schema, get_field(map, member_name));
                    return coerced;
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
    dynamic const& value)
{
    return coerce_value_impl(look_up_named_type, type, value);
}

dynamic
coerce_value(
    std::function<api_type_info(api_named_type_reference const& ref)> const&
        look_up_named_type,
    api_type_info const& type,
    dynamic&& value)
{
    return coerce_value_impl(look_up_named_type, type, std::move(value));
}

} // namespace cradle
