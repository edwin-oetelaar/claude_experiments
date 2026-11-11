"""Parse an HTTP/1.x request head.

Same rules as src/http.c in c_echo_http. One difference in style: C returns a
result code, Python raises an exception that carries the HTTP status.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

MAX_HEAD = 8192
"""Largest request head we accept, terminating blank line included."""

MAX_HEADERS = 32
"""Most header lines we accept in one request."""

REASONS = {
    200: "OK",
    400: "Bad Request",
    404: "Not Found",
    405: "Method Not Allowed",
    431: "Request Header Fields Too Large",
    505: "HTTP Version Not Supported",
}


_HEAD_END = re.compile(rb"\n\r?\n")


class HttpError(Exception):
    """A request we cannot serve; `status` is the HTTP status to answer with."""

    def __init__(self, status: int) -> None:
        super().__init__(f"{status} {REASONS.get(status, 'Unknown')}")
        self.status = status


@dataclass
class Request:
    method: str
    target: str
    version: str
    headers: list[tuple[str, str]] = field(default_factory=list)


def find_head_end(buf: bytes) -> int:
    """Length of the head including the blank line, or 0 if it is not complete yet.

    Accepts "\\r\\n\\r\\n" as the standard says, and also "\\n\\n" so that a
    request typed by hand in `nc` works too: a line end followed by an empty line.
    """
    match = _HEAD_END.search(buf)
    return match.end() if match is not None else 0


def parse_request(head: bytes) -> Request:
    """Parse a complete request head, as found by find_head_end().

    Raises:
        HttpError: 400 for malformed lines, 505 for another HTTP version,
            431 for too many headers.
    """
    # latin-1 maps every byte to one character, so decoding can never fail.
    lines = head.decode("latin-1").split("\n")
    lines = [line.removesuffix("\r") for line in lines]

    parts = lines[0].split(" ")
    if len(parts) != 3 or not parts[0] or not parts[1].startswith("/"):
        raise HttpError(400)
    method, target, version = parts
    if version not in ("HTTP/1.1", "HTTP/1.0"):
        raise HttpError(505 if version.startswith("HTTP/") else 400)

    request = Request(method, target, version)
    for line in lines[1:]:
        if not line:
            break  # the blank line that ends the head
        name, colon, value = line.partition(":")
        if not colon or not name or name[-1] in " \t":
            raise HttpError(400)
        if len(request.headers) == MAX_HEADERS:
            raise HttpError(431)
        request.headers.append((name, value.strip(" \t")))
    return request
