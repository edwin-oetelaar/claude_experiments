"""Contract tests: how an echo_http server must behave, seen from the network.

This file is identical in c_echo_http and py_echo_http. Only conftest.py
differs: it says how to start that project's server. Two implementations that
pass the same tests keep the same promises, whatever language they are in.

The tests talk raw bytes over a socket instead of using an HTTP library, so
that they can also send requests a library would refuse to send.
"""

from __future__ import annotations

import re
import signal
import socket
import subprocess
import time
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parent.parent


@dataclass
class Response:
    status: int
    headers: dict[str, str]  # names in lower case
    body: bytes


@pytest.fixture(scope="module")
def port(server_cmd: list[str]) -> Iterator[int]:
    """Start the server on a free port, yield the port, stop it with Ctrl+C.

    One server serves all tests in this file, as a real server serves many
    clients; the clean stop on Ctrl+C is checked once, at the end.
    """
    proc = subprocess.Popen(
        [*server_cmd, "--port", "0"],
        cwd=PROJECT_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    assert proc.stdout is not None
    line = proc.stdout.readline()
    match = re.match(r"Listening on http://[\d.]+:(\d+)", line)
    if match is None:
        proc.kill()
        pytest.fail(f"no 'Listening on' line, got {line!r}; stderr: {proc.communicate()[1]}")
    yield int(match.group(1))

    proc.send_signal(signal.SIGINT)
    _, stderr = proc.communicate(timeout=5)
    assert proc.returncode == 0, f"server did not stop cleanly on SIGINT:\n{stderr}"


def exchange(port: int, *pieces: bytes, pause: float = 0.0) -> Response:
    """Send the request (optionally in pieces), read until the server closes."""
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        for piece in pieces:
            sock.sendall(piece)
            time.sleep(pause)
        chunks = []
        while data := sock.recv(65536):
            chunks.append(data)
    raw = b"".join(chunks)

    head, sep, body = raw.partition(b"\r\n\r\n")
    assert sep, f"response has no blank line after the headers: {raw[:200]!r}"
    status_line, *header_lines = head.decode("latin-1").split("\r\n")
    version, status, _reason = status_line.split(" ", 2)
    assert version == "HTTP/1.1"
    headers = {}
    for line in header_lines:
        name, _, value = line.partition(":")
        headers[name.strip().lower()] = value.strip()
    return Response(int(status), headers, body)


def test_get_root_shows_request_as_html(port: int) -> None:
    r = exchange(port, b"GET / HTTP/1.1\r\nHost: test\r\nX-Hello: world\r\n\r\n")
    assert r.status == 200
    assert r.headers["content-type"].startswith("text/html")
    assert int(r.headers["content-length"]) == len(r.body)
    assert r.headers["connection"] == "close"
    assert b"X-Hello" in r.body and b"world" in r.body


def test_html_escapes_client_text(port: int) -> None:
    r = exchange(port, b"GET / HTTP/1.1\r\nX-Evil: <script>alert(1)</script>\r\n\r\n")
    assert r.status == 200
    assert b"&lt;script&gt;" in r.body
    assert b"<script>" not in r.body


def test_raw_returns_request_bytes(port: int) -> None:
    request = b"GET /raw HTTP/1.1\r\nHost: test\r\nX-Hello: world\r\n\r\n"
    r = exchange(port, request)
    assert r.status == 200
    assert r.headers["content-type"].startswith("text/plain")
    assert r.body == request


def test_head_has_headers_but_no_body(port: int) -> None:
    r = exchange(port, b"HEAD / HTTP/1.1\r\n\r\n")
    assert r.status == 200
    assert int(r.headers["content-length"]) > 0
    assert r.body == b""


def test_unknown_path_is_404(port: int) -> None:
    assert exchange(port, b"GET /nope HTTP/1.1\r\n\r\n").status == 404


def test_other_method_is_405_with_allow(port: int) -> None:
    r = exchange(port, b"POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello")
    assert r.status == 405
    assert "GET" in r.headers["allow"]


@pytest.mark.parametrize(
    "request_bytes",
    [
        b"HELLO\r\n\r\n",
        b"GET nopath HTTP/1.1\r\n\r\n",
        b"GET / HTTP/1.1\r\nNoColon\r\n\r\n",
    ],
)
def test_malformed_is_400(port: int, request_bytes: bytes) -> None:
    assert exchange(port, request_bytes).status == 400


def test_unsupported_version_is_505(port: int) -> None:
    assert exchange(port, b"GET / HTTP/2.0\r\n\r\n").status == 505


def test_too_many_headers_is_431(port: int) -> None:
    headers = b"".join(b"X-%d: y\r\n" % i for i in range(33))
    assert exchange(port, b"GET / HTTP/1.1\r\n" + headers + b"\r\n").status == 431


def test_head_too_large_is_431(port: int) -> None:
    big = b"X-Big: " + b"a" * 10000 + b"\r\n"
    assert exchange(port, b"GET / HTTP/1.1\r\n" + big + b"\r\n").status == 431


def test_bare_lf_line_endings_work(port: int) -> None:
    """Typed by hand in `nc` (without -C), lines end in \\n only."""
    assert exchange(port, b"GET / HTTP/1.1\nHost: test\n\n").status == 200


def test_request_in_pieces(port: int) -> None:
    """TCP is a stream: one request may arrive in several reads."""
    r = exchange(port, b"GET /ra", b"w HTTP/1.1\r\nHo", b"st: test\r\n\r\n", pause=0.05)
    assert r.status == 200
    assert r.body == b"GET /raw HTTP/1.1\r\nHost: test\r\n\r\n"


def test_request_count_increments(port: int) -> None:
    first = exchange(port, b"GET /raw HTTP/1.1\r\n\r\n")
    second = exchange(port, b"GET /raw HTTP/1.1\r\n\r\n")
    assert int(second.headers["x-request-count"]) == int(first.headers["x-request-count"]) + 1


def test_survives_client_that_sends_nothing(port: int) -> None:
    socket.create_connection(("127.0.0.1", port), timeout=5).close()
    assert exchange(port, b"GET / HTTP/1.1\r\n\r\n").status == 200
