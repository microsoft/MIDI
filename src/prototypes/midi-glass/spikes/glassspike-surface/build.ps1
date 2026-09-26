# Copyright (c) Microsoft Corporation and Contributors.
# Licensed under the MIT License
# ============================================================================
# This is part of Windows MIDI Services
# Further information: https://aka.ms/midi
# ============================================================================
# MIDI Glass phase 0 spike. Nothing here ships.

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'ARM64')]
    [string] $Platform = 'x64',

    [ValidateSet('Build', 'Rebuild', 'Restore')]
    [string] $Target = 'Build'
)

$ErrorActionPreference = 'Stop'

$projectDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $projectDir '..\..\..\..\..')).Path
$inBoxDir = Join-Path $repoRoot 'src\in-box'

# Every project in this repository is built with SolutionDir pointing at src\in-box. The spike
# references the Windows MIDI Services SDK project, so it needs the same thing.
$solutionDir = $inBoxDir.TrimEnd('\') + '\'

$msbuild = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'MSBuild.exe' -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\MSBuild\\Current\\Bin\\amd64\\MSBuild\.exe$' } |
    Select-Object -First 1 -ExpandProperty FullName

if (-not $msbuild)
{
    throw 'Could not find the 64 bit MSBuild host. The 32 bit one fails on parts of this repository.'
}

$project = Join-Path $projectDir 'glassspike-surface.vcxproj'
$log = Join-Path $env:TEMP "glassspike-$Configuration-$Platform.log"

# A brand new project needs a restore before MIDL can resolve the WinUI types.
if (-not (Test-Path (Join-Path $projectDir 'obj\project.assets.json')) -or $Target -eq 'Restore')
{
    Write-Host "Restoring..." -ForegroundColor Cyan
    & $msbuild $project /t:Restore /p:Configuration=$Configuration /p:Platform=$Platform "/p:SolutionDir=$solutionDir" /v:minimal /nologo /nodeReuse:false

    if ($LASTEXITCODE -ne 0) { throw "Restore failed with exit code $LASTEXITCODE." }

    if ($Target -eq 'Restore') { return }
}

$buildTarget = if ($Target -eq 'Restore') { 'Build' } else { $Target }

Write-Host "Building $Configuration $Platform..." -ForegroundColor Cyan
& $msbuild $project /t:$buildTarget /p:Configuration=$Configuration /p:Platform=$Platform "/p:SolutionDir=$solutionDir" /v:minimal /nologo /nodeReuse:false > $log 2>&1
$exitCode = $LASTEXITCODE

Select-String -Path $log -Pattern ': error|: fatal error|warning C' | Select-Object -First 30 | ForEach-Object { $_.Line.Trim() }

if ($exitCode -ne 0)
{
    Write-Host "Build failed. Full log: $log" -ForegroundColor Red
    exit $exitCode
}

$exe = Join-Path $projectDir "..\..\out\glassspike-surface\$Platform\$Configuration\glassspike.exe"

if (Test-Path $exe)
{
    $item = Get-Item $exe
    Write-Host ("Built {0} ({1:N0} bytes, {2})" -f $item.FullName, $item.Length, $item.LastWriteTime) -ForegroundColor Green
}
else
{
    Write-Host "Build reported success but the executable is missing. Log: $log" -ForegroundColor Yellow
}
