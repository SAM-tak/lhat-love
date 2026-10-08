# LÔVE

LÔVE is a fork of [LÖVE](https://github.com/love2d/love) 12.0, a framework for making 2D games, that scripts them in [L^](https://github.com/SAM-tak/lhat) instead of Lua. The engine underneath is still LÖVE's C++. Lua and LuaJIT are gone, replaced by L^, an embeddable, statically type-checked bytecode language.

> [!NOTE]
> Not ready for real use yet. Compatibility with upstream LÖVE and with existing Lua games is not a goal, and anything may change without notice.

## Differences from LÖVE

- A game is `main.lh` (and optionally `conf.lton`), not `main.lua` / `conf.lua`. See [main-lh.md][mainlh].
- `love.thread` is gone. Threads, channels and async come from L^'s standard library (`std.thread`, `std.channel`, `std.async`).
- LuaJIT, lua53, luasocket, enet and luahttps are gone, so there are no networking modules.
- Release jobs target Windows x64, Linux x64, and macOS arm64 / x64.

## Documentation

The design notes are in `docs/porting/`, in Japanese:

- [lua-to-lhat.md][design] — design decisions of the port
- [status.md][status] — which modules are ported
- [main-lh.md][mainlh] — how to write a game
- [AGENT.md][agent] — build variants and porting conventions

## Builds

GitHub Actions builds Windows on every push and pull request. Releases also build Linux x64 and macOS arm64 / x64, and publish only after all platforms pass. Files are in the [releases][releases] section. Each platform carries two builds:

- `relwithdebinfo` — the full engine, with the L^ front end and the debugger. Windows symbols are in a separate archive; Unix builds retain their debug information. This is what games are developed, run and compiled with.
- `vmonly-shipping` — the runtime a compiled game ships on: no front end, no debugger. See [AGENT.md][agent] for how a game is compiled for it.

The builds of a push are also kept for 14 days as artifacts of its [Actions run][workflows].

Windows packages are `.zip` files; Linux and macOS packages are `.tar.gz` files. Linux packages target Ubuntu 24.04 or compatible newer systems: run `bin/love`, with the host providing glibc and graphics drivers. macOS packages require macOS 15 or newer and contain `lhat-love.app`; the command-line executable is `lhat-love.app/Contents/MacOS/love`. Non-system shared libraries are bundled. macOS apps are ad-hoc signed, without Developer ID signing or notarization.

The `release` workflow can also be run manually to build and test without publishing. `build-unix` can be run separately to check just Linux and macOS. Unix jobs use `lhat.rev`, a pinned SDL source revision, and distribution/Homebrew packages whose installed versions are recorded in each archive's `dependencies.txt`.

## Running

```powershell
.\build\love\Release\lovec.exe path\to\game   # a game directory or a .love file
.\build\love\Release\lovec.exe                # no argument: the nogame screen
```

## Test Suite

The L^ test suite is `testing/lh/suite`. It covers every module that has been ported and runs in a single frame:

```powershell
.\build\love\Release\lovec.exe testing\lh\suite
```

It reports how many checks passed, and exits with 0 if all of them did and 1 if not. The other games in `testing/lh/` are smoke tests with their own expected exit codes, which [AGENT.md][agent] lists.

`testing/*.lua` and the readme in `testing/` belong to upstream LÖVE's Lua test suite, kept as the source the L^ suite was ported from. They do not run here.

## Compilation

### Windows

You need:

- Git
- Visual Studio with the C++ toolchain and the CMake tools, plus the "C++ Clang Compiler for Windows" component (not needed with `-Msvc`)
- A checkout of [L^][lhat] next to this repository, at `..\lhat`

```powershell
git clone https://github.com/SAM-tak/lhat ..\lhat   # once
git -C ..\lhat checkout (Get-Content lhat.rev)       # the L^ commit this repository is pinned to
.\scripts\build.ps1
```

The script clones [megasource][megasource] (LÖVE's bundle of Windows dependencies) next to this repository if it is missing, at the commit `megasource.rev` pins, links this repository into it as `libs\love`, then configures and builds with clang-cl and Ninja. The executables end up in `build\love\Release\`.

Options:

- `-Config Debug` — a Debug build.
- `-Shipping` — for distribution: no L^ debugger. Builds into `build-shipping`.
- `-VmOnly` — no L^ front end. Games have to be compiled first with `lovec --compile-game`. Builds into `build-vmonly`.
- `-Msvc` — cl.exe instead of clang-cl. Builds into `build-msvc`.

`-Shipping` and `-VmOnly` can be combined. [AGENT.md][agent] has the details, including how to compile a game for a VM-only build.

### Linux and macOS

Not a target for now. Nothing has been built or tested outside Windows.

## Dependencies

- [L^][lhat] (`..\lhat`, built together with the engine)
- SDL3
- OpenGL 3.3+ / OpenGL ES 3.0+ / Vulkan
- OpenAL
- FreeType
- HarfBuzz
- ModPlug
- Vorbisfile / Ogg
- Theora
- zlib

On Windows, everything except L^ comes from megasource.

[releases]: https://github.com/SAM-tak/lhat-love/releases
[workflows]: https://github.com/SAM-tak/lhat-love/actions
[lhat]: https://github.com/SAM-tak/lhat
[megasource]: https://github.com/love2d/megasource
[agent]: AGENT.md
[design]: docs/porting/lua-to-lhat.md
[status]: docs/porting/status.md
[mainlh]: docs/porting/main-lh.md
