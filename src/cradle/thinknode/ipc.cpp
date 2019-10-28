#include <cradle/thinknode/ipc.hpp>

#include <cradle/encodings/msgpack_internals.hpp>
#include <cradle/io/raw_memory_io.hpp>

namespace cradle {

void
read_message_body(
    thinknode_provider_message* message,
    uint8_t code,
    boost::shared_array<uint8_t> const& body,
    size_t length)
{
    std::cout << "READING MESSAGE BODY!\n";
    raw_input_buffer buffer(body.get(), length);
    raw_memory_reader<raw_input_buffer> reader(buffer);
    switch (static_cast<calc_message_code>(code))
    {
        case calc_message_code::REGISTER:
        {
            thinknode_provider_registration registration;
            registration.protocol = read_int<uint16_t>(reader);
            registration.pid = read_string(reader, 32);
            *message = make_thinknode_provider_message_with_registration(
                std::move(registration));
            break;
        }
        case calc_message_code::PONG:
        {
            auto code = read_string(reader, 32);
            *message = make_thinknode_provider_message_with_pong(code);
            break;
        }
        case calc_message_code::PROGRESS:
        {
            thinknode_provider_progress_update progress;
            progress.value = read_float(reader);
            progress.message = read_string<uint16_t>(reader);
            *message = make_thinknode_provider_message_with_progress(
                std::move(progress));
            break;
        }
        case calc_message_code::RESULT:
        {
            // Allow the dynamic value to claim ownership of the buffer in case
            // it wants to reference data directly from it.
            ownership_holder ownership(body);
            *message = make_thinknode_provider_message_with_result(
                parse_msgpack_value(ownership, buffer.ptr, buffer.size));
            break;
        }
        case calc_message_code::FAILURE:
        {
            thinknode_provider_failure failure;
            failure.code = read_string<uint8_t>(reader);
            failure.message = read_string<uint16_t>(reader);
            *message = make_thinknode_provider_message_with_failure(
                std::move(failure));
            break;
        }
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calc_message_code") << enum_value_info(code));
    }
}

calc_message_code
get_message_code(thinknode_supervisor_message const& message)
{
    switch (get_tag(message))
    {
        case thinknode_supervisor_message_tag::FUNCTION:
            return calc_message_code::FUNCTION;
        case thinknode_supervisor_message_tag::PING:
            return calc_message_code::PING;
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("thinknode_supervisor_message_tag")
                << enum_value_info(static_cast<int>(get_tag(message))));
    }
}

// a buffer that will simply count the number of bytes that passes through it
struct counting_buffer
{
    size_t size = 0;

    void
    write(void const* data, size_t size)
    {
        this->size += size;
    }
};

size_t
measure_msgpack_size(dynamic const& value)
{
    counting_buffer buffer;
    msgpack::packer<counting_buffer> packer(buffer);
    write_msgpack_value(packer, value);
    return buffer.size;
}

template<class Buffer>
void
serialize_message(Buffer& buffer, thinknode_supervisor_message const& message)
{
    raw_memory_writer<Buffer> writer(buffer);
    switch (get_tag(message))
    {
        case thinknode_supervisor_message_tag::FUNCTION:
        {
            auto const& request = as_function(message);
            write_string<uint8_t>(writer, request.name);
            uint16_t n_args
                = boost::numeric_cast<uint16_t>(request.args.size());
            write_int<uint16_t>(writer, n_args);
            for (auto const& arg : request.args)
            {
                // TODO: Add a static check to see if the buffer actually cares
                // about the value of the argument length.
                size_t length = measure_msgpack_size(arg);
                write_int<uint64_t>(writer, uint64_t(length));
                msgpack::packer<Buffer> packer(buffer);
                write_msgpack_value(packer, arg);
            }
            break;
        }
        case thinknode_supervisor_message_tag::PING:
        {
            auto code = as_ping(message);
            write_string_contents(writer, code);
            break;
        }
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("thinknode_supervisor_message_tag")
                << enum_value_info(static_cast<int>(get_tag(message))));
    }
}

size_t
get_message_body_size(thinknode_supervisor_message const& message)
{
    counting_buffer buffer;
    serialize_message(buffer, message);
    return buffer.size;
}

// a buffer that will stream anything it receives over a Boost Asio socket
// (synchronously)
struct asio_buffer
{
    asio_buffer(tcp::socket& socket) : socket_(socket)
    {
    }

    void
    write(void const* data, size_t size)
    {
        boost::asio::write(socket_, boost::asio::buffer(data, size));
    }

    tcp::socket& socket_;
};

void
write_message_body(
    tcp::socket& socket, thinknode_supervisor_message const& message)
{
    asio_buffer buffer(socket);
    serialize_message(buffer, message);
}

} // namespace cradle
