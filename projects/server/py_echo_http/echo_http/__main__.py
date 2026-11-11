"""Command line: python -m echo_http [--host ADDR] [--port N]."""

from __future__ import annotations

import argparse
import asyncio
import sys

from .app import AppState
from .server import serve

DEFAULT_PORT = 8080


def main() -> int:
    parser = argparse.ArgumentParser(prog="echo_http", description=__doc__)
    parser.add_argument(
        "--host",
        default="127.0.0.1",
        help="IPv4 address to listen on (default 127.0.0.1; 0.0.0.0 = all)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"TCP port (default {DEFAULT_PORT}; 0 = any free port)",
    )
    args = parser.parse_args()

    state = AppState()
    try:
        asyncio.run(serve(state, args.host, args.port))
    except KeyboardInterrupt:
        pass  # Ctrl+C: asyncio.run() has already cancelled the server
    except OSError as err:
        print(f"cannot listen on {args.host}:{args.port}: {err}", file=sys.stderr)
        return 1
    print(f"stopped after {state.request_count} request(s)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
