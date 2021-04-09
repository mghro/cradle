#include <cradle/background/os.h>

// LOWER_THREAD_PRIORITY

#ifdef WIN32

#include <windows.h>

namespace cradle {

void
lower_thread_priority(std::thread& thread)
{
    HANDLE win32_thread = thread.native_handle();
    SetThreadPriority(win32_thread, THREAD_PRIORITY_BELOW_NORMAL);
}

} // namespace cradle

#else

namespace cradle {

void
lower_thread_priority(std::thread&)
{
    // TODO
}

} // namespace cradle

#endif
