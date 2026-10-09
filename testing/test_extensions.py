"""Build real C extensions; exercise registration, source isolation and embedded VM signatures."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

REPO = Path(__file__).resolve().parents[1]
CONF = 'window = false^, modules = { audio = false^, graphics = false^ }'
GAME = '''module^extensiontest
import^probe
import^peer
public^let^run = p^{
    if^probe.message() != "native" { return^1 }
    return^probe.answer() + peer.answer()
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lovec', type=Path, default=REPO / 'build/love/Release/lovec.exe')
    parser.add_argument('--vm', type=Path, default=REPO / 'build-vmonly-shipping/love/Release/lovec.exe')
    parser.add_argument('--lhat', type=Path, default=REPO.parent / 'lhat')
    parser.add_argument('--lhat-generated', type=Path, default=REPO / 'build/love/lhat/include')
    args = parser.parse_args()
    compiler, vm = args.lovec.resolve(), args.vm.resolve()
    suffix = '.dll' if sys.platform == 'win32' else '.dylib' if sys.platform == 'darwin' else '.so'

    def command(*items):
        result = subprocess.run(list(map(str, items)), capture_output=True, text=True,
                                encoding='utf-8', errors='replace', timeout=120)
        assert result.returncode == 0, (items, result.stdout, result.stderr)
        return result

    with tempfile.TemporaryDirectory(prefix='lhat-extensions-') as temporary:
        root = Path(temporary)
        fixture = root / 'fixture'
        game = root / 'game with spaces'
        game.mkdir()
        native = game / 'native'
        native.mkdir()
        manifest = game / 'extensions.txt'
        original = '\ufeff# Native extensions\r\n\r\n  native/peer \r\nnative/probe\r\n'
        manifest.write_text(original, encoding='utf-8')
        (game / 'main.lh').write_text(GAME, encoding='utf-8')
        (game / 'conf.lton').write_text(CONF, encoding='utf-8')
        cleanup = root / 'cleanup.log'
        env = dict(os.environ, LHAT_EXTENSION_TEST_LOG=str(cleanup))

        def run(*items, exe=compiler, expected=0, error=None):
            result = subprocess.run([str(exe), '--no-error-screen', *map(str, items)],
                                    cwd=root, env=env, capture_output=True, text=True,
                                    encoding='utf-8', errors='replace', timeout=40)
            assert result.returncode == expected, (items, result.returncode, result.stdout, result.stderr)
            if error:
                assert error in result.stderr, (items, error, result.stderr)
            return result

        def build(header='', peer_header=''):
            command('cmake', '-S', REPO / 'testing/native-extension', '-B', fixture,
                    f'-DLHAT_INCLUDE_DIR={args.lhat.resolve().as_posix()}/include',
                    f'-DLHAT_GENERATED_INCLUDE_DIR={args.lhat_generated.resolve().as_posix()}',
                    f'-DSIGNATURE_HEADER={header}', f'-DPEER_SIGNATURE_HEADER={peer_header}')
            command('cmake', '--build', fixture, '--config', 'Release')
            for name in ('probe', 'peer', 'badabi', 'badversion', 'badsignatures', 'noentry'):
                binary = next(fixture.rglob(name + suffix))
                shutil.copy2(binary, native / binary.name)

        build()
        run(game, expected=84)
        assert sorted(cleanup.read_text().splitlines()) == ['peer disposed', 'probe disposed']
        run(game, exe=vm, expected=1, error='requires an embedded signature table')

        # Diagnostics precede checking the game, including malformed manifests.
        cases = [
            ('native/missing', 'Could not load'),
            ('native/noentry', 'missing lhat_extension_v1'),
            ('native/badabi', 'incompatible extension ABI'),
            ('native/badversion', 'incompatible L^ version'),
            ('../probe', 'expected a relative library path'),
            ('/absolute', 'expected a relative library path'),
            ('C:/absolute', 'expected a relative library path'),
            ('native\\probe', 'expected a relative library path'),
            ('native/probe\nnative/probe' + suffix, 'duplicate library'),
            ('native/probe\0', 'NUL byte'),
        ]
        for text, error in cases:
            manifest.write_text(text, encoding='utf-8')
            run(game, expected=1, error=error)
        manifest.write_text('native/badsignatures', encoding='utf-8')
        run(game, exe=vm, expected=1, error='invalid or incompatible embedded signature table')
        manifest.write_text(original, encoding='utf-8')

        # Full builds can bootstrap an extension with no embedded table.
        def signatures_header(name):
            signatures = root / (name + '.bin')
            run('--dump-signatures', signatures, game)
            header = root / (name + '.h')
            data = signatures.read_bytes()
            header.write_text('static const uint8_t probe_signatures[] = {\n' +
                              '\n'.join(','.join(str(x) for x in data[i:i+24]) + ','
                                        for i in range(0, len(data), 24)) +
                              '\n};\nstatic const size_t probe_signatures_length = sizeof(probe_signatures);\n')
            return header.as_posix()

        peer_header = signatures_header('peer-signatures')
        manifest.write_text('native/probe')
        probe_header = signatures_header('probe-signatures')
        manifest.write_text(original, encoding='utf-8')
        build(probe_header, peer_header)

        api = root / 'host.json'
        run('--dump-host-api', api, game)
        assert 'probe' in api.read_text() and 'peer' in api.read_text()
        json.loads(api.read_text())
        # Parallel LTON compilation creates multiple programs in one process.
        for i in range(3):
            (game / f'data{i}.lton').write_text(f'value = {i}')
        compiled = root / 'compiled'
        run('--compile-game', compiled, '--jobs', 3, game)
        run(compiled, expected=84)
        run(compiled, exe=vm, expected=84)

        def archive(folder, out, omit_manifest=False):
            with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as zipped:
                for path in folder.rglob('*'):
                    relative = path.relative_to(folder)
                    if path.is_file() and relative.parts[0] != 'native':
                        if not (omit_manifest and path.name == 'extensions.txt'):
                            zipped.write(path, relative.as_posix())

        # The manifest is inside the archive, but DLLs remain outside it.
        packed = root / 'game.love'
        archive(compiled, packed)
        run(packed, expected=1, error='Could not load')
        shutil.copytree(native, root / 'native')
        (root / 'extensions.txt').write_text('native/missing')
        run(packed, expected=84)
        run(packed, exe=vm, expected=84)

        # A save file cannot opt into native loading; the second run removes it.
        plain = root / (root.name + '-plain')
        plain.mkdir()
        (plain / 'conf.lton').write_text(CONF)
        (plain / 'main.lh').write_text('''module^plain
import^love.filesystem
public^let^run = p^{
    if^love.filesystem.exists("extensions.txt") {
        love.filesystem.remove("extensions.txt")
    else^:
        love.filesystem.write("extensions.txt", "native/missing") catch^nil^
    }
    return^0
}
''')
        run(plain)
        without_extensions = root / 'without-extensions'
        run('--compile-game', without_extensions, plain)
        assert not (without_extensions / 'extensions.txt').exists()
        run(plain)

        # Restart retains library handles while registering a fresh program.
        (game / 'main.lh').write_text('''module^restarttest
import^love.event
import^probe
public^let^load = p^{
    if^love.event.restartValue() fits^nil^ {
        love.event.restart(1)
    else^:
        love.event.quit(probe.answer())
    }
}
''')
        run(game, expected=42)
        (game / 'main.lh').write_text(GAME)

        if sys.platform == 'win32':
            for engine, label in ((compiler, 'full'), (vm, 'vm')):
                package = root / ('fused-' + label)
                package.mkdir()
                for binary in engine.parent.glob('*.dll'):
                    shutil.copy2(binary, package / binary.name)
                shutil.copytree(native, package / 'native')
                # The default save identity comes from the executable name.
                # Keep it unique so the plain-game probe cannot touch a user's save.
                fused = package / (root.name + '-' + label + '.exe')
                fused.write_bytes(engine.read_bytes() + packed.read_bytes())
                (package / 'extensions.txt').write_text('native/missing')
                run(exe=fused, expected=84)
                # Absence of an embedded list does not enable the external one.
                plain_zip = root / 'plain.love'
                archive(plain, plain_zip)
                if engine == vm:
                    plain_compiled = root / 'plain-compiled'
                    run('--compile-game', plain_compiled, plain)
                    archive(plain_compiled, plain_zip)
                fused.write_bytes(engine.read_bytes() + plain_zip.read_bytes())
                run(exe=fused)
                run(exe=fused)

    print('PASS: native extensions, phases, diagnostics, cleanup, restart, source isolation, archives, LSP and VM signatures')


if __name__ == '__main__':
    main()
