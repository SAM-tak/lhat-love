"""Windows DAP path-map regression; run with --lovec path/to/lovec.exe.

Uses only Python's standard library. The temporary game has no window or audio.
"""

import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time


class Client:
    def __init__(self, connection):
        self.connection = connection
        self.reader = connection.makefile("rb")
        self.sequence = 0
        self.events = []

    def receive(self):
        headers = {}
        while True:
            line = self.reader.readline()
            if not line:
                raise RuntimeError("DAP connection closed")
            if line == b"\r\n":
                break
            key, value = line.decode("ascii").split(":", 1)
            headers[key.lower()] = value.strip()
        return json.loads(self.reader.read(int(headers["content-length"])))

    def request(self, command, arguments=None):
        self.sequence += 1
        payload = json.dumps({"seq": self.sequence, "type": "request",
                              "command": command, "arguments": arguments or {}}).encode()
        self.connection.sendall(
            f"Content-Length: {len(payload)}\r\n\r\n".encode() + payload)
        while True:
            message = self.receive()
            if message.get("type") == "event":
                self.events.append(message)
            elif message.get("request_seq") == self.sequence:
                assert message.get("success"), message
                return message.get("body", {})

    def event(self, name):
        while True:
            for i, message in enumerate(self.events):
                if message.get("event") == name:
                    return self.events.pop(i).get("body", {})
            self.events.append(self.receive())


def check(lovec, root):
    game = root / "Case Game 日本語"
    nested = game / "src" / "シーン" / "game.lh"
    nested.parent.mkdir(parents=True)
    nested.write_text("module^dapgame\npublic^let^answer = f^{\n    return^42\n}\n", encoding="utf-8")
    main = game / "main.lh"
    main.write_text('module^dapmain\nlet^scene = require^"src/シーン/game.lh"\n'
                    'let^value = scene.answer()\n'
                    'public^let^run = p^{ _yield^ return^0 }\n', encoding="utf-8")
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
            for source, line in [(main, 3), (nested, 3)]:
                canonical = str(source)
                spellings = [canonical, str(source.relative_to(game)).swapcase(),
                             canonical.swapcase(), canonical.lower().replace("\\", "/"),
                             canonical[0].lower() + canonical[1:]]
                for spelling in spellings:
                    body = client.request("setBreakpoints", {
                        "source": {"path": spelling}, "breakpoints": [{"line": line}]})
                    assert len(body["breakpoints"]) == 1, body
                    assert body["breakpoints"][0]["verified"], (spelling, body)
                    print("verified:", spelling)
            body = client.request("setBreakpoints", {
                "source": {"path": str(game / "missing.lh")}, "breakpoints": [{"line": 1}]})
            assert not body["breakpoints"][0]["verified"], body
            client.request("configurationDone")
            for expected in [main, nested]:
                stopped = client.event("stopped")
                stack = client.request("stackTrace", {"threadId": stopped["threadId"]})
                assert stopped["reason"] == "breakpoint", (stopped, stack)
                actual = stack["stackFrames"][0]["source"]["path"]
                assert os.path.normcase(actual) == os.path.normcase(str(expected)), stack
                print("stopped:", actual)
                client.request("continue", {"threadId": stopped["threadId"]})
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
    if os.name != "nt":
        parser.exit(message="Skipped: this regression exercises Windows path comparison.\n")
    with tempfile.TemporaryDirectory(prefix="lhat-dap-path-", dir=args.temp_root) as temp:
        check(args.lovec.resolve(), Path(temp))
    print("DAP path mapping passed")
