"""How to start this project's server; test_contract.py does the rest."""

import sys

import pytest


@pytest.fixture(scope="module")
def server_cmd() -> list[str]:
    # Started from the project directory, so `-m echo_http` finds the package.
    return [sys.executable, "-m", "echo_http"]
