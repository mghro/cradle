// FakeIt triggers some warnings on MSVC.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 5030)
#pragma warning(disable : 4068)
#pragma warning(disable : 4702)
#include <fakeit.hpp>
#pragma warning(pop)
#else
#include <fakeit.hpp>
#endif
