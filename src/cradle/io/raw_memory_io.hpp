#ifndef CRADLE_IO_RAW_MEMORY_IO_HPP
#define CRADLE_IO_RAW_MEMORY_IO_HPP

#include <boost/numeric/conversion/cast.hpp>

#include <cradle/common.hpp>
#include <cradle/io/endian.hpp>

// This file provides utilities for reading and writing data to and from
// raw memory buffers.

namespace cradle {

// READING

CRADLE_DEFINE_EXCEPTION(corrupt_data)

struct raw_input_buffer
{
    raw_input_buffer(uint8_t const* ptr, size_t size) : ptr_(ptr), size_(size)
    {
    }

    uint8_t const* ptr_;
    size_t size_;

    uint8_t const*
    data() const
    {
        return ptr_;
    }

    size_t
    size() const
    {
        return size_;
    }

    void
    read(void* dst, size_t size);

    void
    advance(size_t distance)
    {
        this->ptr_ += distance;
        this->size_ -= distance;
    }
};

template<class Buffer>
struct raw_memory_reader
{
    raw_memory_reader(Buffer& buffer) : buffer(buffer)
    {
    }
    Buffer& buffer;
};

template<class Buffer>
void
raw_read(raw_memory_reader<Buffer>& r, void* dst, size_t size)
{
    r.buffer.read(dst, size);
}

template<class Integer, class Buffer>
Integer
read_int(raw_memory_reader<Buffer>& r)
{
    Integer i;
    raw_read(r, &i, sizeof(Integer));
    swap_on_little_endian(&i);
    return i;
}

template<class Buffer>
float
read_float(raw_memory_reader<Buffer>& r)
{
    float f;
    raw_read(r, &f, 4);
    swap_on_little_endian(reinterpret_cast<uint32_t*>(&f));
    return f;
}

template<class Buffer>
string
read_string(raw_memory_reader<Buffer>& r, size_t length)
{
    string s;
    s.resize(length);
    raw_read(r, &s[0], length);
    return s;
}

template<class LengthType, class Buffer>
string
read_string(raw_memory_reader<Buffer>& r)
{
    auto length = read_int<LengthType>(r);
    return read_string(r, length);
}

// WRITING

typedef std::vector<boost::uint8_t> byte_vector;

struct byte_vector_buffer
{
    byte_vector_buffer(byte_vector& bytes) : bytes(&bytes)
    {
    }

    byte_vector* bytes;

    void
    write(char const* src, size_t size)
    {
        size_t current_size = bytes->size();
        bytes->resize(bytes->size() + size);
        std::memcpy(&(*bytes)[0] + current_size, src, size);
    }
};

template<class Buffer>
struct raw_memory_writer
{
    raw_memory_writer(Buffer& buffer) : buffer(buffer)
    {
    }
    Buffer& buffer;
};

template<class Buffer>
void
raw_write(raw_memory_writer<Buffer>& w, void const* src, size_t size)
{
    w.buffer.write(reinterpret_cast<char const*>(src), size);
}

template<class Integer, class Buffer>
void
write_int(raw_memory_writer<Buffer>& w, Integer i)
{
    swap_on_little_endian(&i);
    raw_write(w, &i, sizeof(Integer));
}

template<class Buffer>
void
write_float(raw_memory_writer<Buffer>& w, float f)
{
    swap_on_little_endian(reinterpret_cast<uint32_t*>(&f));
    raw_write(w, &f, 4);
}

// Write the characters in a string, but not its length.
template<class Buffer>
void
write_string_contents(raw_memory_writer<Buffer>& w, string const& s)
{
    raw_write(w, &s[0], s.length());
}

template<class LengthType, class Buffer>
void
write_string(raw_memory_writer<Buffer>& w, string const& s)
{
    auto length = boost::numeric_cast<LengthType>(s.length());
    write_int(w, length);
    write_string_contents(w, s);
}

} // namespace cradle

#endif
