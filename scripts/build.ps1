# Windows build driver for lhatove.
#
# LOVE's Windows build requires the megasource super-project as the CMake
# source directory, with this repository reachable at megasource/libs/love.
# This script clones megasource next to the repo if missing, creates the
# libs/love junction, then configures and builds.
#
# Usage:
#   .\scripts\build.ps1                 # Release, clang-cl, with the debugger
#   .\scripts\build.ps1 -Config Debug
#   .\scripts\build.ps1 -Shipping       # no debugger, into build-shipping
#   .\scripts\build.ps1 -VmOnly         # no front end, into build-vmonly
#   .\scripts\build.ps1 -Msvc           # cl.exe instead, into build-msvc
#
# The compiler is clang-cl by default -- the one Visual Studio ships under
# VC\Tools\Llvm. The interpreter's dispatch is a computed goto under Clang and
# a switch under MSVC (lhat src/vm.c), and that alone makes a dispatch-bound
# loop about a third faster; every workload measured was faster or level.
#
# Clang goes through Ninja rather than the Visual Studio generator: the
# generator's ClangCL platform toolset is a separate installer component,
# and the compiler itself needs none of it -- only the MSVC environment
# (headers, libraries, the linker), which vcvars64.bat puts in this process.
#
# -Shipping is what a distribution is built with: it takes the L^ debugger
# and its DAP adapter out of the binary, the VM's line hook included. A fused
# game is made by appending an archive to the executable this produces. Each
# configuration builds into a directory of its own so none of them share a
# CMake cache.
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Release",
    [string]$MegasourceDir,
    [string]$LhatDir,
    [string]$BuildDir,
    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",
    [switch]$ConfigureOnly,
    [switch]$Shipping,
    [switch]$VmOnly,
    [switch]$Msvc
)

$ErrorActionPreference = "Stop"

if (-not $Msvc -and $Platform -ne "x64") {
    throw "The clang-cl build targets x64. Pass -Msvc to build for $Platform."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$reposParent = Split-Path $repoRoot -Parent
if (-not $MegasourceDir) { $MegasourceDir = Join-Path $reposParent "megasource" }
if (-not $LhatDir) { $LhatDir = Join-Path $reposParent "lhat" }
if (-not $BuildDir) {
    $name = "build"
    if ($Shipping) { $name = "build-shipping" }
    if ($VmOnly) { $name = if ($Shipping) { "build-vmonly-shipping" } else { "build-vmonly" } }
    if ($Msvc) { $name += "-msvc" }
    $BuildDir = Join-Path $repoRoot $name
}

if (-not (Test-Path (Join-Path $LhatDir "include/lhat.h"))) {
    throw "lhat not found at '$LhatDir' (expected include/lhat.h). Pass -LhatDir."
}

if (-not (Test-Path (Join-Path $MegasourceDir "CMakeLists.txt"))) {
    Write-Host "Cloning megasource into $MegasourceDir"
    git clone https://github.com/love2d/megasource.git $MegasourceDir
    if ($LASTEXITCODE -ne 0) { throw "megasource clone failed" }
}

# megasource expects this repo at libs/love; a junction avoids copying and
# needs no admin rights.
$loveLink = Join-Path $MegasourceDir "libs/love"
if (Test-Path $loveLink) {
    $item = Get-Item $loveLink -Force
    if ($item.LinkType -ne "Junction") {
        throw "'$loveLink' exists and is not a junction. Remove it manually."
    }
    if ((Resolve-Path $item.Target).Path -ne $repoRoot) {
        Write-Host "Re-pointing junction $loveLink -> $repoRoot"
        [System.IO.Directory]::Delete($loveLink)
        New-Item -ItemType Junction -Path $loveLink -Target $repoRoot | Out-Null
    }
} else {
    New-Item -ItemType Junction -Path $loveLink -Target $repoRoot | Out-Null
    Write-Host "Created junction $loveLink -> $repoRoot"
}

# CMake will not change the generator of a tree already configured with
# another, and says so late and obscurely. Say it here, and leave deleting
# the tree to whoever owns it.
$generator = if ($Msvc) { "Visual Studio" } else { "Ninja Multi-Config" }
$cache = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cache) {
    $line = Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.*)$' | Select-Object -First 1
    if ($line) {
        $had = $line.Matches[0].Groups[1].Value
        if (-not $had.StartsWith($generator)) {
            throw "$BuildDir was configured with '$had'. Delete it to build there with " +
                  "$(if ($Msvc) { 'MSVC' } else { 'clang-cl' }), or pass -BuildDir."
        }
    }
}

# 09 章: a shipping build carries no debugger at all -- not the adapter, and
# not the VM's line hook that it sits on.
$dap = if ($Shipping) { "OFF" } else { "ON" }
$vm = if ($VmOnly) { "ON" } else { "OFF" }
$common = @("-DLHATOVE_LHAT_DIR=$LhatDir", "-DLHATOVE_WITH_DAP=$dap", "-DLHATOVE_VM_ONLY=$vm")

if ($Msvc) {
    cmake -S $MegasourceDir -B $BuildDir -A $Platform @common
} else {
    # The MSVC environment, into this process: clang-cl compiles, but the
    # headers, the libraries and the resource compiler are MSVC's.
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found. Is Visual Studio installed?" }
    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { throw "No Visual Studio installation with the C/C++ toolchain was found." }
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
    }

    $clang = Join-Path $vsPath "VC\Tools\Llvm\x64\bin\clang-cl.exe"
    if (-not (Test-Path $clang)) {
        throw "clang-cl not found at $clang. Install 'C++ Clang Compiler for Windows' " +
              "in the Visual Studio Installer, or pass -Msvc."
    }
    $ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source
    if (-not $ninja) {
        $ninja = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    }
    if (-not (Test-Path $ninja)) { throw "ninja not found. Install the Visual Studio CMake tools." }

    # Forward slashes: CMake reads a backslash in a -D value as an escape.
    $clang = $clang.Replace("\", "/")
    $ninja = $ninja.Replace("\", "/")
    # CMAKE_NINJA_CMCLDEPS_RC=OFF: Ninja scans a resource script's includes by
    # running it through the C compiler, and clang-cl refuses the UTF-16
    # love.rc that rc.exe itself reads without complaint.
    cmake -S $MegasourceDir -B $BuildDir -G "Ninja Multi-Config" `
        "-DCMAKE_C_COMPILER=$clang" "-DCMAKE_CXX_COMPILER=$clang" `
        "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_NINJA_CMCLDEPS_RC=OFF @common
}
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

if ($ConfigureOnly) { return }

cmake --build $BuildDir --config $Config --target love lovec
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Write-Host ""
Write-Host "Build finished. Executables under: $BuildDir\love\$Config\"
