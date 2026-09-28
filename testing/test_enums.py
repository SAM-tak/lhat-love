"""Host enum regression checks; run with --lovec path/to/lovec.exe."""

import argparse
from pathlib import Path
import subprocess
import tempfile


IMPORTS = """module^enumtest
import^love.graphics
import^love.joystick
import^love.sensor
import^love.audio
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lovec", type=Path, default=Path("build/love/Release/lovec.exe"))
    args = parser.parse_args()
    lovec = args.lovec.resolve()

    cases = {
        "string_draw_mode": 'public^let^draw = p^{ love.graphics.rectangle("fill", 0, 0, 1, 1) }',
        "wrong_draw_enum": 'public^let^draw = p^{ love.graphics.rectangle(love.graphics.ArcMode.pie, 0, 0, 1, 1) }',
        "invented_member": 'public^let^draw = p^{ love.graphics.rectangle(love.graphics.DrawMode.filled, 0, 0, 1, 1) }',
        "string_button": 'let^read = f^j:love.joystick.Joystick -> bool^ { j.isGamepadDown("a") }',
        "axis_as_button": 'let^read = f^j:love.joystick.Joystick -> bool^ { j.isGamepadDown(love.joystick.GamepadAxis.leftx) }',
        "string_filter_tail": 'let^set = p^t:love.graphics.Texture { t.setFilter(love.graphics.FilterMode.linear, "nearest") }',
        "string_time_unit": 'let^read = f^s:love.audio.Source -> number^ { s.tell("seconds") }',
        "string_alignment": 'public^let^draw = p^{ love.graphics.printf("text", 0, 0, 100, "left") }',
    }
    valid = """
public^let^gamepadpressed = p^j:love.joystick.Joystick, button:love.joystick.GamepadButton {}
public^let^gamepadreleased = p^j:love.joystick.Joystick, button:love.joystick.GamepadButton {}
public^let^gamepadaxis = p^j:love.joystick.Joystick, axis:love.joystick.GamepadAxis, value:number^ {}
public^let^joystickhat = p^j:love.joystick.Joystick, index:number^, hat:love.joystick.JoystickHat {}
public^let^sensorupdated = p^sensor:love.sensor.SensorType, x:number^, y:number^, z:number^ {}
public^let^run = p^{ return^0 }
"""
    with tempfile.TemporaryDirectory(prefix="lhatove-enums-") as directory:
        root = Path(directory)
        game = root / "game"
        game.mkdir()
        (game / "conf.lton").write_text(
            "window = false^, modules = { audio = false^, graphics = false^ }\n", encoding="utf-8"
        )

        def invoke(*arguments):
            result = subprocess.run(
                [str(lovec), *map(str, arguments)], capture_output=True, text=True,
                encoding="utf-8", errors="replace", timeout=30,
            )
            return result.returncode, result.stdout + result.stderr

        for name, source in cases.items():
            (game / "main.lh").write_text(IMPORTS + source + "\n", encoding="utf-8")
            code, output = invoke("--compile-game", root / name, game)
            assert code != 0 and "error:" in output, (name, code, output)

        (game / "main.lh").write_text(IMPORTS + valid, encoding="utf-8")
        code, output = invoke("--compile-game", root / "valid", game)
        assert code == 0, output
        # Boot validates the exported callbacks; this also reads binary enum types.
        code, output = invoke(root / "valid")
        assert code == 0, output

        legacy = valid.replace("button:love.joystick.GamepadButton", "button:string^")
        (game / "main.lh").write_text(IMPORTS + legacy, encoding="utf-8")
        code, output = invoke(game)
        assert code != 0 and "gamepadpressed" in output, (code, output)

    print(f"{len(cases)} invalid enum calls rejected; typed binary callbacks accepted; legacy callbacks rejected")


if __name__ == "__main__":
    main()
