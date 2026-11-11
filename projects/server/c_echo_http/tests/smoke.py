"""Start a server, fetch / and /raw with urllib, stop it with Ctrl+C.

Usage: python tests/smoke.py COMMAND [ARGS...]   (the server gets --port 0 appended)
"""

import re
import signal
import subprocess
import sys
import urllib.request


def main() -> int:
    proc = subprocess.Popen([*sys.argv[1:], "--port", "0"], stdout=subprocess.PIPE, text=True)
    assert proc.stdout is not None
    line = proc.stdout.readline().strip()
    print(line)
    match = re.search(r"http://[\d.]+:\d+", line)
    if match is None:
        proc.kill()
        print("server did not start", file=sys.stderr)
        return 1

    try:
        for path in ("/", "/raw"):
            with urllib.request.urlopen(match.group(0) + path, timeout=5) as resp:
                body = resp.read()
                print(f"GET {path} -> {resp.status}, {len(body)} bytes, "
                      f"X-Request-Count {resp.headers['X-Request-Count']}")
    finally:
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=5)
    print(f"server exited with {proc.returncode}")
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
