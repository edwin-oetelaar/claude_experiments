"""How to start this project's server; test_contract.py does the rest."""

import os
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parent.parent


@pytest.fixture(scope="module")
def server_cmd() -> list[str]:
    # ECHO_HTTP_BIN lets `pixi run test-debug` test the sanitizer build.
    return [str(PROJECT_ROOT / os.environ.get("ECHO_HTTP_BIN", "build/c_echo_http"))]
