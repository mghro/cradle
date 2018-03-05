#ifndef CRADLE_CORE_REGULAR_HPP
#define CRADLE_CORE_REGULAR_HPP

#include <boost/concept_check.hpp>

#include <cradle/core/type_interfaces.hpp>
#include <cradle/core/utilities.hpp>

namespace cradle {

// The Regular concept describes CRADLE regular types.
template<class T>
struct Regular : boost::DefaultConstructible<T>,
                 boost::Assignable<T>,
                 boost::CopyConstructible<T>,
                 boost::EqualityComparable<T>,
                 boost::LessThanComparable<T>
{
    BOOST_CONCEPT_USAGE(Regular)
    {
        // T must provide CRADLE type info.
        using cradle::get_type_info;
        check_same_type(cradle::api_type_info(), get_type_info<T>());

        // T must allow querying its size.
        using cradle::deep_sizeof;
        check_same_type(size_t(), deep_sizeof(t));

        // T must allow swapping.
        using std::swap;
        swap(t, t);

        // T must support conversion to and from cradle::dynamic.
        cradle::dynamic v;
        to_dynamic(&v, t);
        from_dynamic(&t, v);
    }

 private:
    T t;

    // Check that the types of the two arguments are the same.
    // (Type deduction will fail if they're not.)
    template<class U>
    void
    check_same_type(U const&, U const&)
    {
    }
};

typedef nil_t regular_archetype;

} // namespace cradle

#endif
