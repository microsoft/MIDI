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

& $msbuild (Join-Path $PSScriptRoot 'midi-mcp-spike.vcxproj') /t:Build /m /nologo /v:minimal `
    "/p:Configuration=$Configuration" "/p:Platform=$Platform"

exit $LASTEXITCODE
