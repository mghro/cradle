#include <cradle/websocket/client.hpp>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include <cradle/encodings/json.hpp>
#include <cradle/websocket/messages.hpp>

namespace cradle {

typedef websocketpp::client<websocketpp::config::asio_client> client_type;

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

struct websocket_client_impl
{
    client_type client;
    websocketpp::connection_hdl server_handle;
};

websocket_client::websocket_client()
{
    impl_ = new websocket_client_impl;
    auto& client = impl_->client;

    client.clear_access_channels(websocketpp::log::alevel::all);
    client.init_asio();
}

websocket_client::~websocket_client()
{
    delete impl_;
}

void
websocket_client::connect(string const& uri)
{
    auto& client = impl_->client;
    websocketpp::lib::error_code ec;
    client_type::connection_ptr server = client.get_connection(uri, ec);
    if (ec)
    {
        CRADLE_THROW(
            websocket_client_error()
            << internal_error_message_info(ec.message()));
    }
    client.connect(server);
    impl_->server_handle = server->get_handle();
}

void
websocket_client::set_message_handler(
    std::function<void(websocket_server_message const& message)> const& handler)
{
    impl_->client.set_message_handler(
        [=](websocketpp::connection_hdl hdl, message_ptr message) {
            handler(from_dynamic<websocket_server_message>(
                parse_json_value(message->get_payload())));
        });
}

void
websocket_client::send(websocket_client_message const& message)
{
    auto json = value_to_json(to_dynamic(message));
    websocketpp::lib::error_code ec;
    impl_->client.send(
        impl_->server_handle, json, websocketpp::frame::opcode::text, ec);
    if (ec)
    {
        CRADLE_THROW(
            websocket_client_error()
            << internal_error_message_info(ec.message()));
    }
}

void
websocket_client::run()
{
    impl_->client.run();
}

void
websocket_client::set_open_handler(std::function<void()> const& handler)
{
    impl_->client.set_open_handler(
        [=](websocketpp::connection_hdl hdl) { handler(); });
}

void
websocket_client::close()
{
    impl_->client.close(
        impl_->server_handle, websocketpp::close::status::normal, "done");
}

} // namespace cradle
