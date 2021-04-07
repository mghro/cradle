#ifndef CRADLE_ENCODINGS_MSGPACK_INTERNALS_H
#define CRADLE_ENCODINGS_MSGPACK_INTERNALS_H

// This file provides a generic implementation of msgpack encodings/decoding on
// dynamic values. (Well, currently only encoding is implemented.)
//
// This takes care of understanding CRADLE dynamic values and interfacing them
// with msgpack-c, but it leaves it up to you to supply the implementation of
// msgpack-c's Buffer concept and initialize the msgpack::packer object. Thus,
// you can potentially use this for streaming and other more interesting forms
// of I/O.
//
// Note that because this must include the msgpack-c header, it indirectly
// includes all sorts of other stuff, includings windows.h on Windows, so use
// with caution.

#include <boost/endian/conversion.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <cradle/encodings/msgpack.h>

// Include msgpack-c, disabling any warnings that it would trigger.
#define MSGPACK_USE_CPP11
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <msgpack.hpp>
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#include <msgpack.hpp>
#pragma warning(pop)
#else
#include <msgpack.hpp>
#endif

namespace cradle {

template<class Buffer>
void
write_msgpack_value(msgpack::packer<Buffer>& packer, dynamic const& v)
{
    switch (v.type())
    {
        case value_type::NIL:
            packer.pack_nil();
            break;
        case value_type::BOOLEAN:
            if (cast<bool>(v))
                packer.pack_true();
            else
                packer.pack_false();
            break;
        case value_type::INTEGER:
            packer.pack_int64(cast<integer>(v));
            break;
        case value_type::FLOAT:
            packer.pack_double(cast<double>(v));
            break;
        case value_type::STRING: {
            auto const& s = cast<string>(v);
            packer.pack_str(boost::numeric_cast<uint32_t>(s.length()));
            packer.pack_str_body(
                s.c_str(), boost::numeric_cast<uint32_t>(s.length()));
            break;
        }
        case value_type::BLOB: {
            blob const& x = cast<blob>(v);
            // Check to make sure that the blob size is within the MessagePack
            // specification's limit.
            if (x.size >= 0x1'00'00'00'00)
            {
                CRADLE_THROW(
                    msgpack_blob_size_limit_exceeded()
                    << msgpack_blob_size_info(x.size)
                    << msgpack_blob_size_limit_info(0x1'00'00'00'00));
            }
            packer.pack_bin(boost::numeric_cast<uint32_t>(x.size));
            packer.pack_bin_body(
                reinterpret_cast<char const*>(x.data),
                boost::numeric_cast<uint32_t>(x.size));
            break;
        }
        case value_type::DATETIME: {
            int8_t const ext_type = 1; // Thinknode datetime ext type
            int64_t t = (cast<ptime>(v) - ptime(date(1970, 1, 1)))
                            .total_milliseconds();
            // We need to use the smallest possible int type to store the
            // datetime.
            if (t >= -0x80 && t < 0x80)
            {
                int8_t x = int8_t(t);
                packer.pack_ext(1, ext_type);
                packer.pack_ext_body(reinterpret_cast<char const*>(&x), 1);
            }
            else if (t >= -0x80'00 && t < 0x80'00)
            {
                int16_t x = int16_t(t);
                boost::endian::native_to_big_inplace(x);
                packer.pack_ext(2, ext_type);
                packer.pack_ext_body(reinterpret_cast<char const*>(&x), 2);
            }
            else if (
                t >= -int64_t(0x80'00'00'00) && t < int64_t(0x80'00'00'00))
            {
                int32_t x = int32_t(t);
                boost::endian::native_to_big_inplace(x);
                packer.pack_ext(4, ext_type);
                packer.pack_ext_body(reinterpret_cast<char const*>(&x), 4);
            }
            else
            {
                int64_t x = int64_t(t);
                boost::endian::native_to_big_inplace(x);
                packer.pack_ext(8, ext_type);
                packer.pack_ext_body(reinterpret_cast<char const*>(&x), 8);
            }
            break;
        }
        case value_type::ARRAY: {
            dynamic_array const& x = cast<dynamic_array>(v);
            size_t size = x.size();
            packer.pack_array(boost::numeric_cast<uint32_t>(size));
            for (size_t i = 0; i != size; ++i)
                write_msgpack_value(packer, x[i]);
            break;
        }
        case value_type::MAP: {
            dynamic_map const& x = cast<dynamic_map>(v);
            packer.pack_map(boost::numeric_cast<uint32_t>(x.size()));
            for (auto const& i : x)
            {
                write_msgpack_value(packer, i.first);
                write_msgpack_value(packer, i.second);
            }
            break;
        }
    }
}

} // namespace cradle

#endif
