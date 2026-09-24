# Collects what a distribution of a finished build consists of.
#
# The engine's own install rules (love/cmake_install.cmake) name the
# executables, liblove.dll, SDL3.dll and the MSVC runtime. The script is run
# directly rather than through `cmake --install`: that goes through
# megasource's top-level rules, which also want LuaJIT's lua51.dll -- built by
# nobody here, since nothing links it.
#
# Two things those rules do not do, done here:
#   - OpenAL32.dll is not among them. Upstream got it from the top-level
#     install, which this does not run.
#   - lhat's own install rules ride along (include/, lib/: the SDK for
#     embedding L^). A game does not need them.
#
# Usage:
#   .\scripts\package.ps1 -Out dist\lhat-love                  # build\, Release
#   .\scripts\package.ps1 -Config RelWithDebInfo -Out dist\a -Symbols dist\a-pdb
#   .\scripts\package.ps1 -BuildDir build-vmonly-shipping -Out dist\b
#
# The configuration has to be built already (scripts\build.ps1 -Config ...).
# -Symbols also collects the .pdb files, which are large and go in a package
# of their own.
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Release",
    [string]$BuildDir,
    [Parameter(Mandatory)][string]$Out,
    [string]$Symbols
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot "build" }
$BuildDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDir)
$Out = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Out)

$installScript = Join-Path $BuildDir "love/cmake_install.cmake"
if (-not (Test-Path $installScript)) {
    throw "'$BuildDir' has no love/cmake_install.cmake. Configure it with scripts\build.ps1 first."
}

$built = Join-Path $BuildDir "love/$Config"
# The engine's DLL is love.dll in Debug and Release, liblove.dll in
# RelWithDebInfo; the install rules spell each of them out.
foreach ($name in "lovec.exe", "love.exe", "SDL3.dll", "OpenAL32.dll") {
    if (-not (Test-Path (Join-Path $built $name))) {
        throw "$name is not in '$built'. Build the $Config configuration first."
    }
}
if (-not ((Test-Path (Join-Path $built "love.dll")) -or (Test-Path (Join-Path $built "liblove.dll")))) {
    throw "The engine's DLL is not in '$built'. Build the $Config configuration first."
}

# Refuse a destination that has something in it: -Out is whatever the caller
# typed, and emptying it would be someone's directory.
foreach ($dir in $Out, $Symbols) {
    if ($dir -and (Test-Path $dir) -and (Get-ChildItem $dir -Force | Select-Object -First 1)) {
        throw "'$dir' already exists and is not empty. Remove it first."
    }
}

# One line per installed file, lhat's headers included, is more than a log
# wants; the listing at the end says what is in the package.
cmake "-DCMAKE_INSTALL_PREFIX=$Out" "-DCMAKE_INSTALL_CONFIG_NAME=$Config" -P $installScript | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Install failed" }

Remove-Item (Join-Path $Out "include"), (Join-Path $Out "lib") -Recurse -Force
Copy-Item (Join-Path $built "OpenAL32.dll") $Out

if ($Symbols) {
    $pdbs = @(Get-ChildItem $built -Filter "*.pdb" -File)
    if (-not $pdbs) { throw "The $Config configuration leaves no .pdb files in '$built'." }
    New-Item -ItemType Directory -Path $Symbols -Force | Out-Null
    Copy-Item $pdbs.FullName $Symbols
}

foreach ($dir in $Out, $Symbols) {
    if (-not $dir) { continue }
    Write-Host ""
    Write-Host "$dir"
    Get-ChildItem $dir -Recurse -File | ForEach-Object {
        Write-Host ("  {0,10:N0} KB  {1}" -f ($_.Length / 1KB), $_.FullName.Substring($dir.Length + 1))
    }
}
