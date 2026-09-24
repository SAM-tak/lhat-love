# Regenerates the three files a VM-only engine carries in place of a front end:
#
#   src/lh/Signatures.h    the signature table: what every registration
#                          declares, as bytes instead of text to parse
#   src/lh/BootBinary.h    the embedded Boot.lh, compiled
#   src/lh/NogameBinary.h  the embedded nogame, compiled
#
# They are made by a full build's lovec, and are checked in. Redo them when a
# registration or an embedded unit changes -- and when lhat's version does:
# a binary carries a fingerprint of LHAT_VERSION and the VM's limits, and a
# VM-only engine refuses a table or a unit that does not carry its own ("the
# signature table this build carries does not fit its registrations").
#
# Usage:
#   .\scripts\regen-generated.ps1                          # build\love\Release\lovec.exe
#   .\scripts\regen-generated.ps1 -Lovec dist\pkg\lovec.exe
#   .\scripts\regen-generated.ps1 -Out some\other\dir      # leaves src\lh alone
param(
    [string]$Lovec,
    [string]$Out,
    # Any game will do: the engine registers everything before it runs one.
    [string]$Game = "testing/lh/hello"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $Lovec) { $Lovec = Join-Path $repoRoot "build/love/Release/lovec.exe" }
if (-not $Out) { $Out = Join-Path $repoRoot "src/lh" }
$Lovec = (Resolve-Path $Lovec).Path
New-Item -ItemType Directory -Path $Out -Force | Out-Null
$Out = (Resolve-Path $Out).Path

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("regen-" + [guid]::NewGuid().ToString("N"))
$emb = Join-Path $work "emb"
New-Item -ItemType Directory -Path $emb -Force | Out-Null

function Invoke-Lovec {
    & $Lovec @args
    if ($LASTEXITCODE -ne 0) { throw "lovec $($args -join ' ') failed (exit $LASTEXITCODE)" }
}

try {
    Push-Location $repoRoot
    Invoke-Lovec --dump-signatures (Join-Path $work "sigs.bin") $Game
    Invoke-Lovec --dump-embedded "$emb\" $Game       # Boot.lh
    Invoke-Lovec --dump-embedded "$emb\"             # nogame, under the name main.lh
} finally {
    Pop-Location
}

$bin2header = Join-Path $PSScriptRoot "bin2header.ps1"
& $bin2header -In (Join-Path $work "sigs.bin") -Out (Join-Path $Out "Signatures.h") -Name lh_signatures
& $bin2header -In (Join-Path $emb "Boot.lh.bin") -Out (Join-Path $Out "BootBinary.h") -Name lh_boot_binary
& $bin2header -In (Join-Path $emb "main.lh.bin") -Out (Join-Path $Out "NogameBinary.h") -Name lh_nogame_binary

Remove-Item -LiteralPath $work -Recurse -Force
