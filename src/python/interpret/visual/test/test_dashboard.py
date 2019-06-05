# Copyright (c) 2019 Microsoft Corporation
# Distributed under the MIT software license

from ..dashboard import AppRunner
from time import sleep


def test_random_port():
    app_runner = AppRunner()

    app_runner.start()
    sleep(1)
    is_alive = app_runner.ping()
    assert is_alive
    app_runner.stop()
