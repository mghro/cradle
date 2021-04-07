#include <cradle/core/type_interfaces.h>

#include <fmt/format.h>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <cradle/core/dynamic.h>
#include <cradle/utilities/text.h>

namespace cradle {

// BOOL

void
to_dynamic(dynamic* v, bool x)
{
    *v = x;
}

void
from_dynamic(bool* x, dynamic const& v)
{
    *x = cast<bool>(v);
}

// STRING

void
to_dynamic(dynamic* v, string const& x)
{
    *v = x;
}

void
from_dynamic(string* x, dynamic const& v)
{
    // Strings are also used to encode datetimes in JSON, so it's possible we
    // might misinterpret a string as a datetime.
    if (v.type() == value_type::DATETIME)
        *x = to_value_string(cast<ptime>(v));
    else
        *x = cast<string>(v);
}

// INTEGERS

#define CRADLE_DEFINE_INTEGER_INTERFACE(T)                                    \
    void to_dynamic(dynamic* v, T x)                                          \
    {                                                                         \
        *v = boost::numeric_cast<integer>(x);                                 \
    }                                                                         \
    void from_dynamic(T* x, dynamic const& v)                                 \
    {                                                                         \
        /* Floats can also be acceptable as integers if they convert          \
         * properly.                                                          \
         */                                                                   \
        if (v.type() == value_type::FLOAT)                                    \
            *x = boost::numeric_cast<T>(cast<double>(v));                     \
        else                                                                  \
            *x = boost::numeric_cast<T>(cast<integer>(v));                    \
    }

CRADLE_DEFINE_INTEGER_INTERFACE(signed char)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned char)
CRADLE_DEFINE_INTEGER_INTERFACE(signed short)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned short)
CRADLE_DEFINE_INTEGER_INTERFACE(signed int)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned int)
CRADLE_DEFINE_INTEGER_INTERFACE(signed long)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned long)
CRADLE_DEFINE_INTEGER_INTERFACE(signed long long)
CRADLE_DEFINE_INTEGER_INTERFACE(unsigned long long)

// FLOATS

#define CRADLE_DEFINE_FLOAT_INTERFACE(T)                                      \
    void to_dynamic(dynamic* v, T x)                                          \
    {                                                                         \
        *v = double(x);                                                       \
    }                                                                         \
    void from_dynamic(T* x, dynamic const& v)                                 \
    {                                                                         \
        /* Integers can also acceptable as floats if they convert properly.   \
         */                                                                   \
        if (v.type() == value_type::INTEGER)                                  \
            *x = boost::numeric_cast<T>(cast<integer>(v));                    \
        else                                                                  \
            *x = boost::numeric_cast<T>(cast<double>(v));                     \
    }

CRADLE_DEFINE_FLOAT_INTERFACE(double)
CRADLE_DEFINE_FLOAT_INTERFACE(float)

// DATE

static date
parse_date(std::string const& s)
{
    namespace bg = boost::gregorian;
    std::istringstream is(s);
    is.imbue(std::locale(
        std::locale::classic(), new bg::date_input_facet("%Y-%m-%d")));
    try
    {
        date d;
        is >> d;
        if (d != date() && !is.fail()
            && is.peek() == std::istringstream::traits_type::eof())
        {
            return d;
        }
    }
    catch (...)
    {
    }
    CRADLE_THROW(
        parsing_error() << expected_format_info("date")
                        << parsed_text_info(s));
}

string
to_string(date const& d)
{
    namespace bg = boost::gregorian;
    std::ostringstream os;
    os.imbue(std::locale(std::cout.getloc(), new bg::date_facet("%Y-%m-%d")));
    os << d;
    return os.str();
}

void
to_dynamic(dynamic* v, date const& x)
{
    *v = to_string(x);
}

void
from_dynamic(date* x, dynamic const& v)
{
    *x = parse_date(cast<string>(v));
}

// PTIME

string
to_string(ptime const& t)
{
    namespace bt = boost::posix_time;
    std::ostringstream os;
    os.imbue(
        std::locale(std::cout.getloc(), new bt::time_facet("%Y-%m-%d %X")));
    os << t;
    return os.str();
}

string
to_value_string(ptime const& t)
{
    namespace bt = boost::posix_time;
    std::ostringstream os;
    os.imbue(
        std::locale(std::cout.getloc(), new bt::time_facet("%Y-%m-%dT%H:%M")));
    os << t;
    // Add the seconds and timezone manually to match Thinknode.
    os << fmt::format(
        ":{:02d}.{:03d}Z",
        t.time_of_day().seconds(),
        t.time_of_day().total_milliseconds() % 1000);
    return os.str();
}

ptime
parse_ptime(std::string const& s)
{
    namespace bt = boost::posix_time;
    std::istringstream is(s);
    is.imbue(std::locale(
        std::cout.getloc(), new bt::time_input_facet("%Y-%m-%dT%H:%M:%s")));
    ptime t;
    is >> t;
    char z;
    is.get(z);
    if (t != ptime() && z == 'Z'
        && is.peek() == std::istringstream::traits_type::eof())
    {
        return t;
    }
    CRADLE_THROW(
        parsing_error() << expected_format_info("datetime")
                        << parsed_text_info(s));
}

void
to_dynamic(dynamic* v, ptime const& x)
{
    *v = x;
}

void
from_dynamic(ptime* x, dynamic const& v)
{
    *x = cast<ptime>(v);
}

// BLOB

bool
operator==(blob const& a, blob const& b)
{
    return a.size == b.size
           && (a.data == b.data || std::memcmp(a.data, b.data, a.size) == 0);
}

bool
operator<(blob const& a, blob const& b)
{
    return a.size < b.size
           || (a.size == b.size && a.data != b.data
               && std::memcmp(a.data, b.data, a.size) < 0);
}

void
to_dynamic(dynamic* v, blob const& x)
{
    *v = x;
}

void
from_dynamic(blob* x, dynamic const& v)
{
    *x = cast<blob>(v);
}

size_t
hash_value(blob const& x)
{
    uint8_t const* bytes = reinterpret_cast<uint8_t const*>(x.data);
    return boost::hash_range(bytes, bytes + x.size);
}

blob
make_string_blob(string s)
{
    blob b;
    // This is a little roundabout, but it seems like the most reasonable way
    // to ensure that a) the string contents don't move if the blob is moved
    // and b) the string contents aren't actually copied if they're large.
    b.ownership = std::make_shared<string>(std::move(s));
    string const& owned_string
        = *std::any_cast<std::shared_ptr<string> const&>(b.ownership);
    b.data = owned_string.c_str();
    b.size = owned_string.length();
    return b;
}

} // namespace cradle
