# PROTOTYPE. Captures every comp in this folder to shots\<name>.png with headless Edge, so the comps
# can be reviewed without opening a browser. Nothing is shown on screen.

param(
    [string] $Edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    [string] $EdgeProfile = (Join-Path $env:TEMP 'midi-ai-comps-edge-profile')
)

$ErrorActionPreference = 'Stop'
$shots = Join-Path $PSScriptRoot 'shots'
New-Item -ItemType Directory -Path $shots -Force | Out-Null

$heights = @{ '4-assistant-settings' = 650 }

foreach ($page in Get-ChildItem $PSScriptRoot -Filter '*.html' | Sort-Object Name) {
    $height = if ($heights.ContainsKey($page.BaseName)) { $heights[$page.BaseName] } else { 850 }
    $png = Join-Path $shots ($page.BaseName + '.png')
    $url = 'file:///' + ($page.FullName -replace '\\', '/')

    # The size is one quoted argument: a bare 1048,$height would be read as a PowerShell array.
    & $Edge --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=1 `
        "--user-data-dir=$EdgeProfile" "--window-size=1048,$height" "--screenshot=$png" --virtual-time-budget=3000 $url 2>$null | Out-Null

    Write-Host ("{0,-32} {1}" -f $page.Name, $(if (Test-Path $png) { 'captured' } else { 'NOT captured' }))
}
