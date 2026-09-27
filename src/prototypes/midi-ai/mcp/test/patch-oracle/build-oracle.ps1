# PROTOTYPE test helper. Builds patch-oracle.exe from MIDI Patchbay's REAL MessageFilter.cpp and
# MessageTransform.cpp, so the harness can check a drafted patch with the code that will run it.
#
# The two sources are copied beside a stand-in pch.h rather than compiled in place: a quoted
# #include "pch.h" looks in the including file's own folder first, and there it would find the
# app's WinUI precompiled header. The copies are hash checked against the repository.

$ErrorActionPreference = 'Stop'

$patchbay = Join-Path $PSScriptRoot '..\..\..\..\..\in-box\user-tools\midi-patchbay' | Resolve-Path
$out = Join-Path $PSScriptRoot '..\..\..\out\oracle'
$src = Join-Path $out 'src'
$projection = Join-Path $PSScriptRoot '..\..\..\out\projection' | Resolve-Path

New-Item -ItemType Directory -Path $src -Force | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'pch.h') $src -Force

foreach ($name in 'MessageFilter.cpp', 'MessageTransform.cpp') {
    Copy-Item (Join-Path $patchbay $name) $src -Force

    if ((Get-FileHash (Join-Path $patchbay $name)).Hash -ne (Get-FileHash (Join-Path $src $name)).Hash) {
        throw "$name copy does not match the repository"
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -prerelease -products * -property installationPath
$devCmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'

$exe = Join-Path $out 'patch-oracle.exe'
if (Test-Path $exe) { Remove-Item $exe -Force }

$oracle = Join-Path $PSScriptRoot 'oracle.cpp'
$batch = Join-Path $out '_build.cmd'

@"
@echo off
call "$devCmd" -arch=amd64 -host_arch=amd64 >nul
cd /d "$out"
cl /nologo /std:c++20 /EHsc /O2 /DNDEBUG /permissive- /utf-8 /bigobj /W4 /I"$src" /I"$patchbay" /I"$projection" "$oracle" "$src\MessageFilter.cpp" "$src\MessageTransform.cpp" /Fe:"$exe" /link windowsapp.lib
"@ | Set-Content -LiteralPath $batch -Encoding Ascii

cmd /c $batch 2>&1 | Where-Object { $_ -match 'error|warning' } | ForEach-Object { Write-Host $_ }

if (-not (Test-Path $exe)) { throw 'patch-oracle did not build' }
Write-Host "built $exe"
