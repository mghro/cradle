#ifndef CRADLE_CORE_FLAGS_H
#define CRADLE_CORE_FLAGS_H

#include <cstddef>

namespace cradle {

// A flag_set is a set of flags, each of which represents a boolean property.
// It is implemented as a simple unsigned integer, where each bit represents
// a different property.
// (The property codes must be defined manually as constants.)
// The advantage of using this over plain unsigned integers is that this is
// type-safe and slightly more explicit.
// A flag_set has a Tag type that identifies the set of properties that go with
// it. Only properties/sets with the same tag can be combined.

// NO_FLAGS can be implicitly converted to any type of flag_set.
struct null_flag_set
{
};
static null_flag_set const NO_FLAGS = null_flag_set();

template<class Tag>
struct flag_set
{
    unsigned code;
    flag_set()
    {
    }
    flag_set(null_flag_set) : code(0)
    {
    }
    explicit flag_set(unsigned code) : code(code)
    {
    }
    // allows use within if statements without other unintended conversions
    typedef unsigned flag_set::*unspecified_bool_type;
    operator unspecified_bool_type() const
    {
        return code != 0 ? &flag_set::code : 0;
    }
};

template<class Tag>
flag_set<Tag>
operator|(flag_set<Tag> a, flag_set<Tag> b)
{
    return flag_set<Tag>(a.code | b.code);
}
template<class Tag>
flag_set<Tag>&
operator|=(flag_set<Tag>& a, flag_set<Tag> b)
{
    a.code |= b.code;
    return a;
}
template<class Tag>
flag_set<Tag>
operator&(flag_set<Tag> a, flag_set<Tag> b)
{
    return flag_set<Tag>(a.code & b.code);
}
template<class Tag>
flag_set<Tag>&
operator&=(flag_set<Tag>& a, flag_set<Tag> b)
{
    a.code &= b.code;
    return a;
}
template<class Tag>
bool
operator==(flag_set<Tag> a, flag_set<Tag> b)
{
    return a.code == b.code;
}
template<class Tag>
bool
operator!=(flag_set<Tag> a, flag_set<Tag> b)
{
    return a.code != b.code;
}
template<class Tag>
bool
operator<(flag_set<Tag> a, flag_set<Tag> b)
{
    return a.code < b.code;
}
template<class Tag>
flag_set<Tag>
operator~(flag_set<Tag> a)
{
    return flag_set<Tag>(~a.code);
}

template<class Tag>
size_t
hash_value(cradle::flag_set<Tag> const& set)
{
    return set.code;
}

#define CRADLE_DEFINE_FLAG_TYPE(type_prefix)                                  \
    struct type_prefix##_flag_tag                                             \
    {                                                                         \
    };                                                                        \
    typedef cradle::flag_set<type_prefix##_flag_tag> type_prefix##_flag_set;

#define CRADLE_DEFINE_FLAG(type_prefix, code, name)                           \
    static unsigned const name##_CODE = code;                                 \
    static cradle::flag_set<type_prefix##_flag_tag> const name(code);

} // namespace cradle

#endif
