"""Compile diagnostics, source output and noninteractive panic regression tests."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lovec", type=Path, default=Path("build/love/Release/lovec.exe"))
    parser.add_argument("--vm-lovec", type=Path, help="also verify compiled LTON with a VM-only build")
    args = parser.parse_args()
    lovec = args.lovec.resolve()
    checks = 0

    def invoke(*arguments, expected=0, executable=lovec):
        nonlocal checks
        result = subprocess.run(
            [str(executable), *map(str, arguments)], capture_output=True,
            text=True, encoding="utf-8", errors="replace", timeout=20,
        )
        assert result.returncode == expected, (arguments, result.returncode, result.stdout, result.stderr)
        checks += 1
        return result

    with tempfile.TemporaryDirectory(prefix="lhatove-cli-") as directory:
        root = Path(directory)
        source = root / "source with spaces"
        (source / "lib").mkdir(parents=True)
        (source / "lib/helper.lh").write_text(
            "module^helper\npublic^let^value = f^ -> number^ { 7 }\n", encoding="utf-8"
        )
        fighter = source / "fighter.lh"
        fighter.write_text(
            'module^fighter\nimport^love.graphics\nlet^helper = require^"lib/helper.lh"\n'
            'print("SOURCE EXECUTED")\n'
            'public^let^value = f^ -> number^ { helper.value() }\n'
            # This is a library function, not the game's draw callback.
            'public^let^draw = p^x:number^ { love.graphics.rectangle(love.graphics.DrawMode.fill, x, 0, 1, 1) }\n',
            encoding="utf-8",
        )
        (source / "unrelated.lh").write_text("this is not valid L^", encoding="utf-8")
        (source / "asset.txt").write_text("asset", encoding="utf-8")
        (source / "conf.lton").write_text("also deliberately invalid", encoding="utf-8")

        output = root / "nested/output with spaces"
        compiled = invoke("--compile", "--debug-names", "-o", output, fighter)
        assert "SOURCE EXECUTED" not in compiled.stdout
        assert {p.relative_to(output).as_posix() for p in output.rglob("*") if p.is_file()} == {
            "fighter.lh", "lib/helper.lh"
        }
        assert (output / "fighter.lh").read_bytes() != fighter.read_bytes()
        (source / "entry.lh").write_text('let^fighter = require^"fighter.lh"\nreturn^fighter.value()\n', encoding="utf-8")
        runnable = root / "runnable"
        invoke("--compile", "-o", runnable, source / "entry.lh")
        (runnable / "conf.lton").write_text(
            "window = false^, modules = { audio = false^, graphics = false^ }\n", encoding="utf-8"
        )
        invoke("--no-error-screen", runnable / "entry.lh", expected=7)
        invoke("--compile", "-o", root / "with-separator", "--", fighter)

        result = invoke("--compile", "--unknown-option", "-o", root / "unknown-option", fighter, expected=1)
        assert "Unknown option --unknown-option" in result.stderr, result.stderr

        for arguments in [
            ("--compile", fighter), ("--compile", "-o"),
            ("--compile", "-o", root / "missing-input"),
            ("--compile", "-o", root / "not-a-file", source),
            ("--compile", "-o", root / "missing", source / "absent.lh"),
            ("-o", root / "orphan", fighter), ("--compile-game",),
        ]:
            invoke(*arguments, expected=1)

        lton = source / "settings.lton"
        lton.write_text("value = 17\n", encoding="utf-8")
        lton_output = root / "nested/lton output"
        invoke("--compile", "--debug-names", "-o", lton_output, lton)
        assert {p.name for p in lton_output.iterdir()} == {"settings.lton"}
        assert (lton_output / "settings.lton").read_bytes() != lton.read_bytes()
        bad_lton = source / "bad.lton"
        bad_lton.write_text("value =\n", encoding="utf-8")
        result = invoke("--compile", "-o", root / "bad-lton", bad_lton, expected=1)
        assert "bad.lton" in result.stderr, result.stderr
        invoke("--compile", "-o", root / "missing-lton", source / "absent.lton", expected=1)

        lton_game = root / "lton game"
        (lton_game / "data/nested").mkdir(parents=True)
        (lton_game / "conf.lton").write_text(
            "window = false^, modules = { audio = false^, graphics = false^ }\n", encoding="utf-8"
        )
        (lton_game / "data/nested/settings.lton").write_bytes(lton.read_bytes())
        (lton_game / "asset.txt").write_text("preserved", encoding="utf-8")
        (lton_game / "main.lh").write_text(
            'module^ltongame\nimport^std.lton\npublic^let^run = p^{\n'
            '    let^data = std.lton.load("data/nested/settings.lton")\n'
            '    if^data fits^t^{ value : number^ } { return^data.value }\n'
            '    return^1\n}\n', encoding="utf-8"
        )
        lton_packaged = root / "lton packaged"
        invoke("--compile-game", lton_packaged, lton_game)
        for relative in ("conf.lton", "data/nested/settings.lton"):
            assert (lton_packaged / relative).read_bytes() != (lton_game / relative).read_bytes()
        assert (lton_packaged / "asset.txt").read_text() == "preserved"
        lton_runners = [lovec]
        if args.vm_lovec:
            lton_runners.append(args.vm_lovec.resolve())
        for executable in lton_runners:
            invoke("--no-error-screen", lton_packaged, expected=17, executable=executable)
        # The standalone output must also be readable through std.lton.load.
        (lton_packaged / "data/nested/settings.lton").write_bytes((lton_output / "settings.lton").read_bytes())
        for executable in lton_runners:
            invoke("--no-error-screen", lton_packaged, expected=17, executable=executable)
        (lton_game / "data/nested/bad.lton").write_bytes(bad_lton.read_bytes())
        result = invoke("--compile-game", root / "bad-lton-game", lton_game, expected=1)
        assert "data/nested/bad.lton" in result.stderr, result.stderr

        # A compiler limit, not a parse/type error: previously only the generic
        # "Could not compile the program." message was shown on this path.
        overflow = "module^overflow\npublic^let^run = p^{\n" + "".join(
            f"    var^v{i} = {i}\n" for i in range(260)
        ) + "    return^0\n}\n"
        game = root / "game"
        game.mkdir()
        (game / "main.lh").write_text(overflow, encoding="utf-8")
        for arguments in [
            ("--no-error-screen", game),
            ("--compile", "-o", root / "overflow-source", game / "main.lh"),
            ("--compile-game", root / "overflow-game", game),
        ]:
            result = invoke(*arguments, expected=1)
            assert "main.lh" in result.stderr and "too many registers or constants" in result.stderr, result.stderr

        (game / "main.lh").write_text("module^game\npublic^let^run = p^{ return^0 }\n", encoding="utf-8")
        extra = game / "extra.lh"
        extra.write_text("let^value:number^ = \"wrong\"\n", encoding="utf-8")
        result = invoke("--compile-game", root / "bad-extra", game, expected=1)
        assert "extra.lh" in result.stderr and "error:" in result.stderr, result.stderr
        extra.write_text(overflow, encoding="utf-8")
        result = invoke("--compile-game", root / "overflow-extra", game, expected=1)
        assert "extra.lh" in result.stderr and "too many registers or constants" in result.stderr, result.stderr
        extra.unlink()
        (game / "asset.txt").write_text("preserved", encoding="utf-8")
        packaged = root / "packaged"
        invoke("--compile-game", packaged, game)
        assert (packaged / "asset.txt").read_text() == "preserved"

        # A windowed panic must finish on its own. Cover root, direct run and
        # resumed callback failures, for both console and GUI executables.
        panic_sources = [
            'panic^"panic probe"\n',
            'module^panicgame\npublic^let^run = p^{ panic^"panic probe" }\n',
            'module^panicgame\npublic^let^update = p^dt:number^ { panic^"panic probe" }\n',
        ]
        for text in panic_sources:
            (game / "main.lh").write_text(text, encoding="utf-8")
            for executable in (lovec, lovec.with_name("love.exe")):
                result = invoke("--no-error-screen", game, expected=1, executable=executable)
                assert "panic probe" in result.stderr and ("main.lh" in result.stderr or "line " in result.stderr), result.stderr

        # Without the option, the existing error screen still waits for input.
        process = subprocess.Popen([str(lovec), str(game)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            try:
                process.communicate(timeout=3)
                raise AssertionError("the default error screen exited without user input")
            except subprocess.TimeoutExpired:
                checks += 1
        finally:
            if process.poll() is None:
                process.terminate()
            _, stderr = process.communicate(timeout=10)
        assert b"panic probe" in stderr, stderr

    print(f"{checks} CLI checks passed")


if __name__ == "__main__":
    main()
