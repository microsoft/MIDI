# PROTOTYPE. Captures every comp in this folder to shots\<name>.png with headless Edge, so the comps
# can be reviewed without opening a browser window. Nothing is shown on screen.
#
# Each page gives its own size in <meta name="size" content="width,height">.
#
#   pwsh -File capture.ps1            every page
#   pwsh -File capture.ps1 -Page 3    pages whose name starts with 3

param(
    [string] $Page = '',
    [string] $Edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    [string] $EdgeProfile = (Join-Path $env:TEMP 'midi-monitor-comps-edge-profile')
)

$ErrorActionPreference = 'Stop'
$shots = Join-Path $PSScriptRoot 'shots'
New-Item -ItemType Directory -Path $shots -Force | Out-Null

foreach ($file in Get-ChildItem $PSScriptRoot -Filter '*.html' | Sort-Object Name) {
    if ($Page -and -not $file.BaseName.StartsWith($Page)) {
        continue
    }

    $width = 1300
    $height = 1000
    $size = [regex]::Match((Get-Content $file.FullName -Raw), '<meta name="size" content="(\d+),(\d+)">')

    if ($size.Success) {
        $width = [int]$size.Groups[1].Value
        $height = [int]$size.Groups[2].Value
    }

    $png = Join-Path $shots ($file.BaseName + '.png')
    $url = 'file:///' + ($file.FullName -replace '\\', '/')

    if (Test-Path $png) {
        Remove-Item $png
    }

    # The size is one quoted argument: a bare 1300,1000 would be read as a PowerShell array.
    $windowSize = '--window-size={0},{1}' -f $width, $height

    & $Edge --headless=new --disable-gpu --hide-scrollbars --disable-lcd-text --force-device-scale-factor=1 `
        "--user-data-dir=$EdgeProfile" $windowSize "--screenshot=$png" --virtual-time-budget=3000 $url 2>$null | Out-Null

    # names only: a printed .png path comes back to the agent as a picture
    Write-Host ("{0,-30} {1}x{2} {3}" -f $file.BaseName, $width, $height, $(if (Test-Path $png) { 'captured' } else { 'NOT captured' }))
}
