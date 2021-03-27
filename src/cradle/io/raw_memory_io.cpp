#include <cradle/io/raw_memory_io.hpp>

#include <cstring>

namespace cradle {

void
raw_input_buffer::read(void* dst, size_t size)
{
    if (size > this->size_)
        throw corrupt_data();
    std::memcpy(dst, this->ptr_, size);
    this->advance(size);
}

} // namespace cradle
