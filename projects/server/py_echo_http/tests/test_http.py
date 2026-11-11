"""Unit tests for the parser and the responses, without a network.

The same cases as tests/http_test.c in c_echo_http.
"""

import pytest

from echo_http.app import AppState, respond
from echo_http.http import MAX_HEADERS, HttpError, find_head_end, parse_request


def test_find_head_end() -> None:
    assert find_head_end(b"GET / HTTP/1.1\r\nHost: x\r\n\r\n") == 27
    assert find_head_end(b"GET / HTTP/1.1\n\n") == 16
    assert find_head_end(b"GET / HTTP/1.1\r\nHost: x\r\n") == 0  # not complete yet
    assert find_head_end(b"") == 0


def test_parse_ok() -> None:
    req = parse_request(b"GET /a?b=1 HTTP/1.1\r\nHost: example\r\nX-Pad:   spaced  \r\n\r\n")
    assert (req.method, req.target, req.version) == ("GET", "/a?b=1", "HTTP/1.1")
    assert req.headers == [("Host", "example"), ("X-Pad", "spaced")]


@pytest.mark.parametrize(
    ("head", "status"),
    [
        (b"HELLO\r\n\r\n", 400),
        (b"GET nopath HTTP/1.1\r\n\r\n", 400),
        (b"GET / HTTP/1.1 extra\r\n\r\n", 400),
        (b"GET / HTTP/1.1\r\nNoColon\r\n\r\n", 400),
        (b"GET / HTTP/1.1\r\nName : v\r\n\r\n", 400),
        (b"GET / HTTP/2.0\r\n\r\n", 505),
        (b"GET / HTTP/1.1\r\n" + b"X: y\r\n" * (MAX_HEADERS + 1) + b"\r\n", 431),
    ],
)
def test_parse_errors(head: bytes, status: int) -> None:
    with pytest.raises(HttpError) as err:
        parse_request(head)
    assert err.value.status == status


def test_responses() -> None:
    st = AppState(server_name="test")

    out = respond(st, b"GET / HTTP/1.1\r\nX-Evil: <script>\r\n\r\n")
    assert out.startswith(b"HTTP/1.1 200 OK\r\n")
    assert b"X-Request-Count: 1\r\n" in out
    assert b"&lt;script&gt;" in out
    assert b"<script>" not in out

    raw = b"GET /raw HTTP/1.1\r\n\r\n"
    out = respond(st, raw)
    assert b"X-Request-Count: 2\r\n" in out
    assert out.endswith(raw)  # body is the request

    out = respond(st, b"POST / HTTP/1.1\r\n\r\n")
    assert out.startswith(b"HTTP/1.1 405 ")
    assert b"Allow: GET, HEAD\r\n" in out

    out = respond(st, b"HEAD /missing HTTP/1.1\r\n\r\n")
    assert out.startswith(b"HTTP/1.1 404 ")
    assert out.endswith(b"\r\n\r\n")  # HEAD: headers only, no body
