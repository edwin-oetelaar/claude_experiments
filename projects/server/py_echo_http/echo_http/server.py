"""The network side, with asyncio.

Unlike c_echo_http this server is concurrent from the start: every
connection runs in its own coroutine, and while one waits for data, asyncio
runs the others. All on one thread, so AppState needs no lock: a coroutine
only gives way at an `await`, and there is none inside app.respond().
"""

from __future__ import annotations

import asyncio
import contextlib
import sys

from .app import AppState, respond, respond_error
from .http import MAX_HEAD, find_head_end

READ_TIMEOUT_S = 5.0
"""A client that sends nothing is dropped after this."""

DRAIN_TIMEOUT_S = 0.5
"""How long to wait for a client to close after the response."""


async def _read_head(reader: asyncio.StreamReader) -> tuple[bytes, int] | None:
    """Read until the blank line that ends the head, or until MAX_HEAD bytes.

    Returns (bytes read, head length; 0 if the head did not fit), or None if
    the client went away or stayed silent.
    """
    buf = b""
    # TCP is a byte stream: one request may arrive in several pieces.
    while len(buf) < MAX_HEAD:
        try:
            data = await asyncio.wait_for(reader.read(MAX_HEAD - len(buf)), READ_TIMEOUT_S)
        except (TimeoutError, ConnectionError):
            return None
        if not data:
            return None
        buf += data
        if head_len := find_head_end(buf):
            return buf, head_len
    return buf, 0


async def _drain(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
    """Close our sending side and read away what the client still sends.

    Closing with unread data makes the kernel send a TCP reset, and the client
    may lose the response. Same reasoning as s_drain() in c_echo_http.
    """
    with contextlib.suppress(OSError):
        writer.write_eof()
    with contextlib.suppress(TimeoutError, ConnectionError):
        async with asyncio.timeout(DRAIN_TIMEOUT_S):
            while await reader.read(1024):
                pass


async def handle_client(
    state: AppState, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
) -> None:
    try:
        result = await _read_head(reader)
        if result is None:
            return
        buf, head_len = result
        out = respond(state, buf[:head_len]) if head_len else respond_error(state, 431)
        writer.write(out)
        await writer.drain()
        await _drain(reader, writer)

        request_line = buf.split(b"\n", 1)[0].rstrip(b"\r")[:60].decode("latin-1")
        print(f"{request_line} -> {out[9:12].decode()}", file=sys.stderr)
    except ConnectionError:
        pass  # the client went away; nothing to answer
    finally:
        writer.close()
        with contextlib.suppress(ConnectionError):
            await writer.wait_closed()


async def serve(state: AppState, host: str, port: int) -> None:
    server = await asyncio.start_server(
        lambda r, w: handle_client(state, r, w), host, port, reuse_address=True
    )
    # With port 0 the kernel picks a port; ask which one it chose.
    bound_port = server.sockets[0].getsockname()[1]
    print(f"Listening on http://{host}:{bound_port}", flush=True)
    async with server:
        await server.serve_forever()
