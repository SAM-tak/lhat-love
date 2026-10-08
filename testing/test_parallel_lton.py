"""Native LTON worker correctness, archive input, diagnostics and CLI validation."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lovec', type=Path, default=Path('build/love/Release/lovec.exe'))
    parser.add_argument('--vm', type=Path, default=Path('build-vmonly-shipping/love/Release/love.exe'))
    args = parser.parse_args()
    compiler, vm = args.lovec.resolve(), args.vm.resolve()

    def run(*arguments, expected=0, exe=compiler):
        # Unix uses the GUI executable too; invalid CLI arguments must not
        # open a modal error dialog while the test waits for an exit code.
        result = subprocess.run([str(exe), '--no-error-screen', *map(str, arguments)], capture_output=True,
                                encoding='utf-8', errors='replace', timeout=40)
        assert result.returncode == expected, (arguments, result.returncode, result.stdout, result.stderr)
        return result

    def contents(folder):
        return {p.relative_to(folder).as_posix(): p.read_bytes()
                for p in folder.rglob('*') if p.is_file()}

    with tempfile.TemporaryDirectory(prefix='lhat-lton-threads-') as temporary:
        root = Path(temporary)
        source = root / 'source with spaces'
        source.mkdir()
        (source / 'main.lh').write_text('''module^paralleltest
import^std.lton
public^let^run = p^{
    let^data = std.lton.load("日本語 0/settings.lton")
    if^data fits^t^{ value : number^ } { return^data.value }
    return^1
}
''', encoding='utf-8')
        # Empty and single-item queues must also work with large requested counts.
        empty = root / 'empty'
        run('--compile-game', empty, '--jobs', 6, source)
        (source / 'conf.lton').write_text(
            'window = false^, modules = { audio = false^, graphics = false^ }', encoding='utf-8')
        single = run('--compile-game', root / 'single', '--jobs', 256, source)
        assert 'with 1 threads' in single.stdout
        for i in range(24):
            folder = source / f'日本語 {i}'
            folder.mkdir()
            # CastFailure identity is shared; parsing/checking/serialization must
            # only read it once worker threads have started.
            (folder / 'settings.lton').write_text(
                'value = ((f^ -> any^ { return^ "bad" })() as^ number^ catch^ 17), values = {' +
                ','.join(map(str, range(200 + i))) + '}', encoding='utf-8')
        (source / 'asset.txt').write_text('unchanged asset', encoding='utf-8')
        archive = root / 'source.love'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as zipped:
            for name, data in contents(source).items():
                zipped.writestr(name, data)
        for debug in (False, True):
            options = ['--debug-names'] if debug else []
            baseline = root / f'serial-{debug}'
            run('--compile-game', baseline, '--jobs', 1, *options, source)
            for iteration, input_path in enumerate((source, archive, source)):
                output = root / f'parallel-{debug}-{iteration}'
                result = run('--compile-game', output, '--jobs', 6, *options, input_path)
                assert '25 LTON files with 6 threads' in result.stdout
                assert contents(output) == contents(baseline)
            run(output, exe=vm, expected=17)

        for value in ('-1', '257', 'x', '1.5', '999999999999999999999'):
            result = run('--compile-game', root / 'invalid', '--jobs', value, source, expected=1)
            assert '--jobs' in result.stderr
        run('--jobs', expected=1)
        run('--jobs', 6, source, expected=1)
        run('--compile', '-o', root / 'invalid-source', '--jobs', 6, source / 'main.lh', expected=1)

        (source / 'bad.lton').write_text('value =', encoding='utf-8')
        result = run('--compile-game', root / 'bad', '--jobs', 6, source, expected=1)
        assert 'bad.lton' in result.stderr
        (source / 'bad.lton').unlink()
        blocked = root / 'blocked'
        blocked.mkdir()
        (blocked / 'conf.lton').mkdir()
        result = run('--compile-game', blocked, '--jobs', 6, source, expected=1)
        assert 'conf.lton' in result.stderr
    print('PASS: native threads, binary equality, debug names, archive input, VM execution, errors and CLI')


if __name__ == '__main__':
    main()
