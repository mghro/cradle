#ifndef CRADLE_BACKGROUND_OS_HPP
#define CRADLE_BACKGROUND_OS_HPP

#include <thread>

// This file defines some thread-related functions that are still require
// OS-specific implementations at this point.

namespace cradle {

// Set the priority of the given thread to lower than normal.
void
lower_thread_priority(std::thread& thread);

} // namespace cradle

#endif
