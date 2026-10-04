# Pins a commit this repository builds against: writes a checkout's HEAD into
# <name>.rev at the repository root, which CI and releases check out.
#
#   lhat.rev        L^, built from source beside this repository (../lhat)
#   megasource.rev  LOVE's bundle of Windows dependencies (../megasource)
#
# A local build takes whatever those checkouts hold. The .rev files are what
# make a CI run, and so a release, reproducible: the same commit of this
# repository always builds against the same L^ and the same dependencies.
#
# Usage:
#   .\scripts\pin.ps1 lhat                   # pin ../lhat's HEAD
#   .\scripts\pin.ps1 megasource
#   .\scripts\pin.ps1 lhat -Dir D:\lhat
#
# It refuses a checkout with uncommitted changes, and a HEAD that is not on
# the remote: CI fetches the commit from there.
#
# After moving lhat's pin, rebuild and run scripts\regen-generated.ps1 -- the
# generated files carry lhat's version, and CI fails when they do not match
# the pinned commit -- then commit lhat.rev and src\lh together.
param(
    [Parameter(Mandatory, Position = 0)]
    [ValidateSet("lhat", "megasource")]
    [string]$Name,
    [string]$Dir
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $Dir) { $Dir = Join-Path (Split-Path $repoRoot -Parent) $Name }
$marker = if ($Name -eq "lhat") { "include/lhat.h" } else { "CMakeLists.txt" }
if (-not (Test-Path (Join-Path $Dir $marker))) {
    throw "$Name not found at '$Dir' (expected $marker). Pass -Dir."
}

$dirty = git -C $Dir status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0) { throw "'$Dir' is not a git checkout." }
if ($dirty) {
    throw "'$Dir' has uncommitted changes. Commit and push them, or pin from a clean checkout."
}

$head = (git -C $Dir rev-parse HEAD).Trim()
git -C $Dir fetch --quiet origin
if ($LASTEXITCODE -ne 0) { throw "Could not fetch $Name's remote to check the commit is there." }
$remotes = git -C $Dir branch --remotes --contains $head
if (-not $remotes) {
    throw "$Name $($head.Substring(0, 9)) is not on the remote yet. Push it first; CI fetches it from there."
}

$file = Join-Path $repoRoot "$Name.rev"
$old = if (Test-Path $file) { (Get-Content $file -TotalCount 1).Trim() } else { "" }
if ($old -eq $head) {
    Write-Host "$Name.rev already pins $($head.Substring(0, 9))."
    return
}
[System.IO.File]::WriteAllText($file, "$head`n")
$from = if ($old) { $old.Substring(0, 9) } else { "(none)" }
Write-Host "$Name.rev: $from -> $($head.Substring(0, 9)) ($(git -C $Dir log -1 --format=%s $head))"
if ($Name -eq "lhat") {
    Write-Host "Now rebuild, run scripts\regen-generated.ps1, and commit lhat.rev with src\lh."
} else {
    Write-Host "Now rebuild to check it, and commit megasource.rev."
}
