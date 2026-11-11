"""What the server answers: from request bytes to response bytes. No sockets here.

Routes (the same as c_echo_http):
    GET /     HTML page that shows the request, with every header escaped
    GET /raw  the request head exactly as received, as text/plain
    HEAD      same as GET, without the body
    other     404 for unknown paths, 405 for other methods
"""

from __future__ import annotations

import html
from dataclasses import dataclass

from .http import REASONS, HttpError, Request, parse_request

STYLE = (
    "body{font-family:system-ui,sans-serif;background:#111;color:#ddd;"
    "max-width:60rem;margin:2rem auto;padding:0 1rem}"
    "code,td{font-family:ui-monospace,monospace}"
    "table{border-collapse:collapse;width:100%}"
    "td{border-bottom:1px solid #333;padding:.3rem .6rem;vertical-align:top;word-break:break-all}"
    "td:first-child{color:#8f8;white-space:nowrap}a{color:#8cf}"
)


@dataclass
class AppState:
    server_name: str = "py_echo_http"
    request_count: int = 0
    """Responses sent so far; shown in X-Request-Count."""


def _finish(
    state: AppState,
    status: int,
    content_type: str,
    body: bytes,
    *,
    with_body: bool = True,
    extra_headers: str = "",
) -> bytes:
    """Status line and headers, then the body unless with_body is False (HEAD)."""
    state.request_count += 1
    head = (
        f"HTTP/1.1 {status} {REASONS[status]}\r\n"
        f"Server: {state.server_name}\r\n"
        f"Content-Type: {content_type}\r\n"
        f"Content-Length: {len(body)}\r\n"
        f"X-Request-Count: {state.request_count}\r\n"
        "Connection: close\r\n"
        f"{extra_headers}"
        "\r\n"
    )
    return head.encode("latin-1") + (body if with_body else b"")


def _plain(state: AppState, status: int, *, with_body: bool = True, extra: str = "") -> bytes:
    body = f"{status} {REASONS[status]}\n".encode()
    return _finish(
        state, status, "text/plain; charset=utf-8", body, with_body=with_body, extra_headers=extra
    )


def _page(state: AppState, req: Request, *, with_body: bool) -> bytes:
    # html.escape: client text must never be inserted into a page as-is (XSS).
    rows = "".join(
        f"<tr><td>{html.escape(name)}</td><td>{html.escape(value)}</td></tr>\n"
        for name, value in req.headers
    )
    request_line = html.escape(f"{req.method} {req.target} {req.version}")
    page = (
        '<!doctype html>\n<html><head><meta charset="utf-8"><title>Echo</title>\n'
        f"<style>{STYLE}</style></head>\n"
        f"<body><h1>Echo from {state.server_name}</h1>\n"
        f"<p>You are request number <b>{state.request_count + 1}</b>. Your browser sent:</p>\n"
        f"<p><code>{request_line}</code></p>\n"
        f"<table>\n{rows}</table>\n"
        '<p>See the exact bytes: <a href="/raw">/raw</a></p>\n'
        "</body></html>\n"
    )
    return _finish(state, 200, "text/html; charset=utf-8", page.encode(), with_body=with_body)


def respond(state: AppState, head: bytes) -> bytes:
    """Build the complete response to the request head `head`."""
    try:
        req = parse_request(head)
    except HttpError as err:
        return respond_error(state, err.status)

    if req.method not in ("GET", "HEAD"):
        return _plain(state, 405, extra="Allow: GET, HEAD\r\n")
    with_body = req.method == "GET"
    if req.target == "/":
        return _page(state, req, with_body=with_body)
    if req.target == "/raw":
        return _finish(state, 200, "text/plain; charset=utf-8", head, with_body=with_body)
    return _plain(state, 404, with_body=with_body)


def respond_error(state: AppState, status: int) -> bytes:
    """An error response that needs no parsed request, e.g. 431 for a head that is too big."""
    return _plain(state, status)
