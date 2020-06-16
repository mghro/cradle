import sys
import os
import subprocess
import msgpack
import time
import websocket
import random
import thinknode
import pytest


def create_connection_robustly(url):
    """Attempt to connect to a server, retrying for a brief period if it doesn't initially work."""
    attempts_left = 100
    while True:
        try:
            return websocket.create_connection(url)
        except ConnectionRefusedError:
            time.sleep(0.01)
            attempts_left -= 1
            if attempts_left == 0:
                raise


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def test_websocket_server():
    deploy_dir = os.environ["CRADLE_DEPLOY_DIR"]

    server_process = \
        subprocess.Popen(
            [deploy_dir + "/server"], cwd=deploy_dir,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    try:
        ws = create_connection_robustly("ws://localhost:41071")

        ws.send_binary(
            msgpack.packb(
                {
                    "request_id": "no_id",
                    "content": {
                        "registration": {
                            "name": "Channing",
                            "session": {
                                "api_url": "",
                                "access_token": "",
                            }
                        }
                    }
                },
                use_bin_type=True))
        ws.send_binary(
            msgpack.packb(
                {
                    "request_id": "id0",
                    "content": {
                        "test": {
                            "message": "Hello, Tigger!"
                        }
                    }
                },
                use_bin_type=True))
        response = msgpack.unpackb(ws.recv(), use_list=False, raw=False)
        assert response == {
            "request_id": "id0",
            "content": {
                "test": {
                    "message": "Hello, Tigger!",
                    "name": "Channing"
                }
            }
        }

        session = thinknode.Session()

        string_object = \
            ("Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
            "Nulla sodales lectus cursus lectus tempor, ut accumsan enim ultrices. "
            "Praesent elit est, lobortis a lacinia at, posuere non lectus.")
        string_iss_id = session.post_iss_object("string", string_object)
        assert session.get_iss_object(string_iss_id) == string_object

        integer_iss_id = session.post_iss_object("integer", 4)
        assert session.get_iss_object(integer_iss_id) == 4

        # Test that an integer is successfully coerced to a float.
        float_iss_id = session.post_iss_object("float", 4)
        assert session.get_iss_object(float_iss_id) == 4.0

        # Test the metadata for the float object.
        float_metadata = session.get_iss_object_metadata(float_iss_id)
        assert float_metadata["Thinknode-Type"] == "float"

        # Also test a named type so that we know that lookup is working.
        box_iss_id = session.post_iss_object("named/mgh/dosimetry/box_2d", {
            "corner": (-1, 0),
            "size": (3, 3)
        })
        assert session.get_iss_object(box_iss_id) == \
            {"corner": (-1, 0), "size": (3, 3)}

        a = random.random()
        b = random.random()
        c = random.random()

        test_calc = \
            {
                "let": {
                    "variables": {
                        "x": {
                            "function": {
                                "account": "mgh",
                                "app": "dosimetry",
                                "name": "addition",
                                "args": [
                                    {
                                        "value": a
                                    },
                                    {
                                        "value": b
                                    }
                                ]
                            }
                        }
                    },
                    "in": {
                        "function": {
                            "account": "mgh",
                            "app": "dosimetry",
                            "name": "addition",
                            "args": [
                                {
                                    "variable": "x",
                                },
                                {
                                    "value": c
                                }
                            ]
                        }
                    }
                }
            }

        test_calc_id = session.post_calc(test_calc)

        assert session.get_iss_object(test_calc_id) == pytest.approx(a + b + c)

        ws.send_binary(
            msgpack.packb({
                "request_id": "no_id",
                "content": {
                    "kill": None
                }
            },
                          use_bin_type=True))
        ws.close()
    except:
        # Check on the server.
        out, err = server_process.communicate(timeout=1)
        eprint("--- server stdout")
        eprint(out)
        eprint("--- server stderr")
        eprint(err)
        server_result = server_process.poll()
        eprint("--- server result")
        eprint(server_result)
        raise

    assert server_process.wait() == 0
