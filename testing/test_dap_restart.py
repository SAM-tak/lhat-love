"""DAP across love.event.restart; run with --lovec path/to/lovec.exe.

The first run stops at a breakpoint and restarts; the debugger stays
connected, hears the old thread exit and a new one start, and the same
breakpoint stops the second run, which quits with 0. Only one "terminated"
comes, at the very end. Uses only Python's standard library. The temporary
game has no window or audio.
"""

import argparse
from pathlib import Path
import socket
import subprocess
import tempfile
import time

from test_dap_paths import Client


GAME = """module^dapgame
import^love.event
var^frames = 0
let^carried = love.event.restartValue()
public^let^update = p^dt:number^{
    frames += 1
    if^frames = 3 {
        let^mark = frames
        if^carried fits^nil^{ love.event.restart(1) else^: love.event.quit(0) }
    }
}
"""
LINE = 8


def check(lovec, root):
    game = root / "game"
    game.mkdir()
    main = game / "main.lh"
    main.write_text(GAME, encoding="utf-8")
    (game / "conf.lton").write_text(
        "window = false^, modules = { audio = false^, graphics = false^ }\n",
        encoding="utf-8")
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    with (root / "engine.log").open("wb") as log:
        process = subprocess.Popen([str(lovec), f"--dap={port}", str(game)],
                                   stdout=log, stderr=log)
        connection = None
        client = None
        try:
            deadline = time.monotonic() + 20
            while connection is None:
                if process.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("Engine did not open DAP: " +
                                       (root / "engine.log").read_text(errors="replace"))
                try:
                    connection = socket.create_connection(("127.0.0.1", port), timeout=1)
                except OSError:
                    time.sleep(0.05)
            connection.settimeout(15)
            client = Client(connection)
            client.request("initialize", {"adapterID": "lhat", "linesStartAt1": True,
                                          "columnsStartAt1": True, "pathFormat": "path"})
            client.request("launch", {})
            body = client.request("setBreakpoints", {
                "source": {"path": str(main)}, "breakpoints": [{"line": LINE}]})
            assert body["breakpoints"][0]["verified"], body
            client.request("configurationDone")

            first = client.event("stopped")
            assert first["reason"] == "breakpoint", first
            print("first run stopped, thread", first["threadId"])
            client.request("continue", {"threadId": first["threadId"]})

            # The restart: the old thread goes, a new one comes, no end.
            while True:
                exited = client.event("thread")
                if exited["reason"] == "exited" and exited["threadId"] == first["threadId"]:
                    break
            started = client.event("thread")
            assert started["reason"] == "started", started
            assert not any(e.get("event") == "terminated" for e in client.events), client.events
            print("restarted on thread", started["threadId"])

            second = client.event("stopped")
            assert second["reason"] == "breakpoint", second
            assert second["threadId"] == started["threadId"], (second, started)
            stack = client.request("stackTrace", {"threadId": second["threadId"]})
            assert stack["stackFrames"][0]["line"] == LINE, stack
            print("second run stopped at the same breakpoint")
            client.request("continue", {"threadId": second["threadId"]})

            exited = client.event("exited")
            assert exited["exitCode"] == 0, exited
            client.event("terminated")
            assert process.wait(timeout=10) == 0
        finally:
            if client is not None:
                client.reader.close()
            if connection is not None:
                connection.close()
            if process.poll() is None:
                process.kill()
                process.wait()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lovec", type=Path, required=True)
    parser.add_argument("--temp-root", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="lhat-dap-restart-", dir=args.temp_root) as temp:
        check(args.lovec.resolve(), Path(temp))
    print("DAP across restart passed")
