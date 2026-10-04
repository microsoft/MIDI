# PROTOTYPE. Captures every comp in this folder to shots\<name>.png with headless Edge, so the comps
# can be reviewed without opening a browser window. Nothing is shown on screen.
#
#   pwsh -File capture.ps1            every page
#   pwsh -File capture.ps1 -Page 2    pages whose name starts with 2

param(
    [string] $Page = '',
    [string] $Edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    [string] $EdgeProfile = (Join-Path $env:TEMP 'midi-patchbay-comps-edge-profile')
)

$ErrorActionPreference = 'Stop'
$shots = Join-Path $PSScriptRoot 'shots'
New-Item -ItemType Directory -Path $shots -Force | Out-Null

# Width and height of the whole page, caption and notes included.
$sizes = @{
    '1-library'    = @(1304, 880)
    '2-editor'     = @(1724, 1190)
    '3-converted'  = @(1500, 820)
    '4-note-filter' = @(1304, 1000)
    '5-message-mask' = @(1304, 1180)
    '6-inspectors' = @(1304, 790)
    'index'        = @(1100, 760)
}

foreach ($file in Get-ChildItem $PSScriptRoot -Filter '*.html' | Sort-Object Name) {
    if ($Page -and -not $file.BaseName.StartsWith($Page)) {
        continue
    }

    $size = if ($sizes.ContainsKey($file.BaseName)) { $sizes[$file.BaseName] } else { @(1304, 900) }
    $png = Join-Path $shots ($file.BaseName + '.png')
    $url = 'file:///' + ($file.FullName -replace '\\', '/')

    if (Test-Path $png) {
        Remove-Item $png
    }

    # The size is one quoted argument: a bare 1304,900 would be read as a PowerShell array.
    $windowSize = '--window-size={0},{1}' -f $size[0], $size[1]

    & $Edge --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=1 `
        "--user-data-dir=$EdgeProfile" $windowSize "--screenshot=$png" --virtual-time-budget=3000 $url 2>$null | Out-Null

    Write-Host ("{0,-24} {1}" -f $file.Name, $(if (Test-Path $png) { 'captured' } else { 'NOT captured' }))
}
