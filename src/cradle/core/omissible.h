#ifndef CRADLE_CORE_OMISSIBLE_H
#define CRADLE_CORE_OMISSIBLE_H

#include <cradle/core/type_interfaces.h>

namespace cradle {

template<class T>
optional<T>
as_optional(omissible<T> const& omis)
{
    return omis ? some(*omis) : optional<T>();
}

template<class T>
bool
operator==(omissible<T> const& a, omissible<T> const& b)
{
    return a ? (b && *a == *b) : !b;
}
template<class T>
bool
operator!=(omissible<T> const& a, omissible<T> const& b)
{
    return !(a == b);
}
template<class T>
bool
operator<(omissible<T> const& a, omissible<T> const& b)
{
    return b && (a ? *a < *b : true);
}
template<class T>
struct type_info_query<omissible<T>> : type_info_query<T>
{
};
template<class T>
size_t
deep_sizeof(omissible<T> const& x)
{
    return sizeof(omissible<T>) + (x ? deep_sizeof(*x) : 0);
}
template<class T>
void
read_field_from_record(
    omissible<T>* field_value,
    dynamic_map const& record,
    std::string const& field_name)
{
    // If the field doesn't appear in the record, just set it to none.
    dynamic const* dynamic_field_value;
    if (get_field(&dynamic_field_value, record, field_name))
    {
        try
        {
            T value;
            from_dynamic(&value, *dynamic_field_value);
            *field_value = value;
        }
        catch (boost::exception& e)
        {
            cradle::add_dynamic_path_element(e, field_name);
            throw;
        }
    }
    else
    {
        *field_value = none;
    }
}
template<class T>
void
write_field_to_record(
    dynamic_map& record,
    string const& field_name,
    omissible<T> const& field_value)
{
    // Only write the field to the record if it has a value.
    if (field_value)
        write_field_to_record(record, field_name, *field_value);
}

template<class T>
void
to_dynamic(dynamic* v, omissible<T> const& x)
{
    dynamic_map record;
    if (x)
    {
        to_dynamic(&record[dynamic("some")], *x);
    }
    else
    {
        to_dynamic(&record[dynamic("none")], cradle::nil);
    }
    *v = std::move(record);
}

CRADLE_DEFINE_EXCEPTION(invalid_omissible_type_tag)
CRADLE_DEFINE_ERROR_INFO(string, omissible_type_tag)

template<class T>
void
from_dynamic(omissible<T>* x, dynamic const& v)
{
    dynamic_map const& record = cradle::cast<cradle::dynamic_map>(v);
    string tag;
    from_dynamic(&tag, cradle::get_union_tag(record));
    if (tag == "some")
    {
        T t;
        from_dynamic(&t, cradle::get_field(record, "some"));
        *x = t;
    }
    else if (tag == "none")
    {
        *x = none;
    }
    else
    {
        CRADLE_THROW(
            invalid_omissible_type_tag() << omissible_type_tag_info(tag));
    }
}
template<class T>
std::ostream&
operator<<(std::ostream& s, omissible<T> const& x)
{
    if (x)
        s << *x;
    else
        s << "none";
    return s;
}

template<class T>
size_t
hash_value(cradle::omissible<T> const& x)
{
    return x ? cradle::invoke_hash(*x) : 0;
}

} // namespace cradle

#endif
