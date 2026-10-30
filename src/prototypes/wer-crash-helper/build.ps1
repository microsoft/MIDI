# PROTOTYPE. Builds the three WER crash helper prototype binaries into out\<platform>\<configuration>\.
# See README.md. One project at a time, at below-normal priority, at most 4 compilers.

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

foreach ($project in 'module\WerHelperModule.vcxproj', 'helper\WerCrashHelper.vcxproj', 'crashtest\WerCrashTest.vcxproj') {
    & $msbuild (Join-Path $PSScriptRoot $project) /t:Build -low /p:CL_MPCount=4 /nologo /v:minimal `
        "/p:Configuration=$Configuration" "/p:Platform=$Platform"

    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Write-Host "Built into $(Join-Path $PSScriptRoot "out\$Platform\$Configuration")"
