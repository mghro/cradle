#ifndef CRADLE_CACHE_TYPES_HPP
#define CRADLE_CACHE_TYPES_HPP

#include <cradle/common.hpp>

namespace cradle {

api(struct)
struct disk_cache_config
{
    optional<std::string> directory;
    integer size_limit;
};

api(struct)
struct disk_cache_info
{
    // the directory where the cache is stored
    std::string directory;

    // the number of entries currently stored in the cache
    integer entry_count;

    // the total size (in bytes)
    integer total_size;
};

api(struct)
struct disk_cache_entry
{
    // the key for the entry
    std::string key;

    // the internal numeric ID of the entry within the cache
    integer id;

    // true iff the entry is stored directly in the database
    bool in_db;

    // the value associated with the entry - This may be omitted, depending
    // on how the entry is stored in the cache and how this info was
    // queried.
    omissible<std::string> value;

    // the size of the entry (in bytes)
    integer size;

    // a 32-bit CRC of the contents of the entry
    uint32_t crc32;
};

} // namespace cradle

#endif
