#include <cradle/websocket/client.hpp>
#include <cradle/websocket/server.hpp>

#include <thread>

#include <cradle/core/testing.hpp>

#include <cradle/encodings/base64.hpp>
#include <cradle/websocket/messages.hpp>

#include "io/http_requests.hpp"

using namespace cradle;

TEST_CASE("websocket client/server", "[ws]")
{
    auto config = make_server_config(
        none, some(string(find_testing_cacert_file().string())), none, 41072);
    websocket_server server(config);
    server.listen();
    std::thread server_thread([&]() { server.run(); });

    optional<websocket_test_response> test_response;

    {
        websocket_client client;
        client.set_message_handler(
            [&](websocket_server_message const& message) {
                switch (get_tag(message.content))
                {
                    case server_message_content_tag::CACHE_RESPONSE: {
                        REQUIRE(message.request_id == "id0");
                        auto response = as_cache_response(message.content);
                        REQUIRE(response.key == "test_key");
                        REQUIRE(response.value == some(string("test_value")));
                        client.send(make_websocket_client_message(
                            "id1",
                            make_client_message_content_with_test(
                                websocket_test_query{"Hello, Patches!"})));
                        break;
                    }
                    case server_message_content_tag::TEST: {
                        REQUIRE(message.request_id == "id1");
                        test_response = as_test(message.content);
                        client.send(make_websocket_client_message(
                            "id2",
                            make_client_message_content_with_kill(nil)));
                        client.close();
                        break;
                    }
                    default:
                        FAIL("unexpected message");
                }
            });
        client.set_open_handler([&]() {
            client.send(make_websocket_client_message(
                "no_id",
                make_client_message_content_with_registration(
                    make_websocket_registration_message(
                        "Kasey", make_thinknode_session("", "")))));
            client.send(make_websocket_client_message(
                "no_id",
                make_client_message_content_with_cache_insert(
                    make_websocket_cache_insert("test_key", "test_value"))));
            client.send(make_websocket_client_message(
                "id0",
                make_client_message_content_with_cache_query("test_key")));
        });
        client.connect("ws://localhost:41072");
        client.run();
    }

    server_thread.join();

    REQUIRE(test_response);
    REQUIRE(test_response->name == "Kasey");
    REQUIRE(test_response->message == "Hello, Patches!");
}
