"""UTF-8 compiler paths, assets and dump outputs on every platform."""

import argparse
from pathlib import Path
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lovec", type=Path, default=Path("build/love/Release/lovec.exe"))
    parser.add_argument("--vm", type=Path)
    args = parser.parse_args()
    compiler = args.lovec.resolve()

    def run(*arguments, executable=compiler, expected=0):
        result = subprocess.run(
            [str(executable), "--no-error-screen", *map(str, arguments)],
            capture_output=True, encoding="utf-8", errors="replace", timeout=40,
        )
        assert result.returncode == expected, (arguments, result.stdout, result.stderr)

    def files(folder):
        return {p.relative_to(folder).as_posix(): p.read_bytes()
                for p in folder.rglob("*") if p.is_file()}

    with tempfile.TemporaryDirectory(prefix="lhat-unicode-") as temporary:
        root = Path(temporary)
        source = root / "入力 ゲーム"
        (source / "あやね").mkdir(parents=True)
        (source / "あやね/cpu.lton").write_text("value = 17\n", encoding="utf-8")
        (source / "あやね/技.lh").write_text(
            "module^helper\npublic^let^value = f^{ return^17 }\n", encoding="utf-8")
        (source / "あやね/画像.dat").write_bytes(b"asset\x00\xff")
        (source / "main.lh").write_text(
            'module^unicodegame\nimport^std.lton\n'
            'let^helper = require^"あやね/技.lh"\n'
            'public^let^run = p^{\n'
            '    let^data = std.lton.load("あやね/cpu.lton")\n'
            '    if^data fits^t^{ value : number^ } {\n'
            '        if^data.value = helper.value() { return^data.value }\n'
            '    }\n    return^1\n}\n', encoding="utf-8")
        (source / "conf.lton").write_text(
            "window = false^, modules = { audio = false^, graphics = false^ }\n",
            encoding="utf-8")
        original = files(source)
        archive = root / "入力 ゲーム.love"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zipped:
            for name, data in original.items():
                zipped.writestr(name, data)

        for index, (input_path, jobs) in enumerate(((source, 1), (source, 6), (archive, 6))):
            output = root / f"出力 {index}/配布"
            run("--compile-game", output, "--jobs", jobs, input_path)
            compiled = files(output)
            assert compiled.keys() == original.keys(), compiled.keys()
            for name, data in original.items():
                if name.endswith((".lh", ".lton")):
                    assert compiled[name] != data, name
                else:
                    assert compiled[name] == data, name
            run(output, expected=17)
            if args.vm:
                run(output, executable=args.vm.resolve(), expected=17)

        standalone = root / "単体 出力/データ"
        run("--compile", "-o", standalone, source / "あやね/cpu.lton")
        assert set(files(standalone)) == {"cpu.lton"}
        assert (standalone / "cpu.lton").read_bytes() != original["あやね/cpu.lton"]
        units = root / "単体 出力/ソース"
        run("--compile", "-o", units, source / "main.lh")
        assert set(files(units)) == {"main.lh", "あやね/技.lh"}

        for option, name in (("--dump-host-api", "登録.json"), ("--dump-signatures", "署名.bin")):
            output = root / name
            run(option, output, source)
            assert output.is_file() and output.stat().st_size > 0, output
        embedded = root / "埋め込み 出力"
        run("--dump-embedded", embedded, source)
        assert (embedded / "Boot.lh.bin").stat().st_size > 0

        blocked = root / "書込不可"
        blocked.write_bytes(b"file, not directory")
        run("--compile-game", blocked / "出力", source, expected=1)

    print("PASS: Unicode source/output paths, serial/parallel LTON, archives, assets and dumps")


if __name__ == "__main__":
    main()
