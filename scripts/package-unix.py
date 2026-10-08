"""Package a native Unix build, including non-system shared dependencies."""
import argparse
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys


def run(*args):
    return subprocess.check_output(args, text=True)


def linux(build, package):
    executable = package / "bin/love"
    executable.parent.mkdir(parents=True)
    libraries = package / "lib"
    libraries.mkdir()
    shutil.copy2(build / "love", executable)
    # Keep the host's libc, loader and graphics driver stack together.
    system = re.compile(r"^(ld-linux|lib(c|m|pthread|dl|rt|resolv|util)\.so|"
                        r"lib(GL|EGL|GLX|GLdispatch|OpenGL|drm|gbm))")
    pending = [build / "love"]
    copied = set()
    while pending:
        source = pending.pop()
        dependencies = run("ldd", str(source))
        if "not found" in dependencies:
            raise RuntimeError(dependencies)
        for name, location in re.findall(r"^\s*(\S+) => (/\S+) \(", dependencies, re.M):
            if name in copied or system.match(name):
                continue
            copied.add(name)
            shutil.copy2(location, libraries / name)
            pending.append(Path(location))
    for library in libraries.iterdir():
        subprocess.run(["patchelf", "--set-rpath", "$ORIGIN", str(library)], check=True)
    subprocess.run(["patchelf", "--set-rpath", "$ORIGIN/../lib", str(executable)], check=True)
    # Retain distribution copyright notices for bundled dependencies.
    notices = package / "licenses/system"
    notices.mkdir(parents=True)
    for notice in Path("/usr/share/doc").glob("*/copyright"):
        if notice.is_file():
            shutil.copy2(notice, notices / (notice.parent.name + ".txt"))


def macos(build, package, deps):
    app = package / "lhat-love.app"
    contents = app / "Contents"
    executable = contents / "MacOS/love"
    executable.parent.mkdir(parents=True)
    shutil.copy2(build / "love", executable)
    with (contents / "Info.plist").open("wb") as output:
        plistlib.dump({
            "CFBundleExecutable": "love",
            "CFBundleIdentifier": "org.lhat.love",
            "CFBundleName": "LÔVE",
            "CFBundlePackageType": "APPL",
            "CFBundleVersion": "12.0",
            "CFBundleShortVersionString": "12.0",
            "LSMinimumSystemVersion": "15.0",
            "NSHighResolutionCapable": True,
        }, output)
    subprocess.run([
        "dylibbundler", "-b", "-cd", "-x", str(executable),
        "-d", str(contents / "Frameworks"), "-p", "@executable_path/../Frameworks/",
        "-s", str(build), "-s", str(deps / "lib"),
    ], check=True)
    # Reject accidental references to Homebrew or the build workspace.
    for binary in [executable, *sorted((contents / "Frameworks").glob("*.dylib"))]:
        for line in run("otool", "-L", str(binary)).splitlines()[1:]:
            dependency = line.strip().split(" (", 1)[0]
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue
            if dependency.startswith("@executable_path/"):
                resolved = executable.parent / dependency[len("@executable_path/"):]
            elif dependency.startswith("@loader_path/"):
                resolved = binary.parent / dependency[len("@loader_path/"):]
            else:
                raise RuntimeError(f"Unbundled dependency in {binary}: {dependency}")
            if not resolved.is_file():
                raise RuntimeError(f"Missing bundled dependency: {resolved}")
    subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
    cellar = Path(run("brew", "--cellar").strip())
    for notice in cellar.rglob("*"):
        if notice.is_file() and notice.name.lower().startswith(("license", "copying", "copyright")):
            target = package / "licenses/homebrew" / notice.relative_to(cellar)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(notice, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("package", type=Path)
    parser.add_argument("--deps", required=True, type=Path)
    args = parser.parse_args()
    build, package = args.build.resolve(), args.package.resolve()
    package.mkdir(parents=True, exist_ok=False)
    (package / "licenses").mkdir()
    if sys.platform == "darwin":
        macos(build, package, args.deps.resolve())
    elif sys.platform == "linux":
        linux(build, package)
    else:
        raise SystemExit("Only Linux and macOS are supported")
    repo = Path(__file__).resolve().parent.parent
    for name in ("license.txt", "readme.md", "changes.txt"):
        shutil.copy2(repo / name, package / name)
    shutil.copy2(args.deps.parent / "SDL/LICENSE.txt", package / "licenses/SDL.txt")


if __name__ == "__main__":
    main()
