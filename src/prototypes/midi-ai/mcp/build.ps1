# PROTOTYPE. Builds midi-mcp-spike.exe into ..\out\<platform>\<configuration>\.
#
# Needs an in-box build to have run once before: the Windows MIDI Services SDK winmd and the
# restored C++/WinRT and WIL packages are borrowed from src\in-box, never rebuilt from here.

param(
    [ValidateSet('x64', 'ARM64')] [string] $Platform = 'x64',
    [ValidateSet('Release', 'Debug')] [string] $Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1

if (-not $msbuild) {
    throw 'MSBuild was not found.'
}

# A running MCP host locks the exe and the SDK DLL. Windows lets a file in use be renamed, so move
# it aside; old copies are deleted once nothing has them open.
$outDir = Join-Path $PSScriptRoot "..\out\$Platform\$Configuration"
Get-ChildItem $outDir -Filter '*.old' -ErrorAction SilentlyContinue | Remove-Item -ErrorAction SilentlyContinue

foreach ($name in 'midi-mcp-spike.exe', 'Windows.Devices.Midi2.dll', 'Windows.Devices.Midi2.pri') {
    $path = Join-Path $outDir $name
    if (-not (Test-Path $path)) { continue }
    try { [IO.File]::Open($path, 'Open', 'ReadWrite', 'None').Dispose() }
    catch { Rename-Item $path ('{0}.{1:yyyyMMddHHmmss}.old' -f $name, (Get-Date)); Write-Host "$name is in use, so it was moved aside. Restart the server in the MCP host to pick up the new build." }
}

& $msbuild (Join-Path $PSScriptRoot 'midi-mcp-spike.vcxproj') /t:Build /m /nologo /v:minimal `
    "/p:Configuration=$Configuration" "/p:Platform=$Platform"

exit $LASTEXITCODE
