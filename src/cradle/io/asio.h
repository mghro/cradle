#ifndef CRADLE_IO_ASIO_H
#define CRADLE_IO_ASIO_H

// Boost ASIO complains if we don't define this.
#if defined(WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601 // Windows 7
#endif
#include <boost/asio.hpp>

#endif
