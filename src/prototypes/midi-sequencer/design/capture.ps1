# PROTOTYPE. Captures every comp in this folder to shots\<name>.png with headless Edge, so the comps
# can be reviewed without opening a browser window. Nothing is shown on screen.
#
# Each page is opened once with ?check to learn its own size, then captured at exactly that size.
# A page with <meta name="variants" content="light"> is also captured with ?theme=light, to
# shots\<name>-light.png.
#
#   pwsh -File capture.ps1            every page
#   pwsh -File capture.ps1 -Page 2    pages whose name starts with 2

param(
    [string] $Page = '',
    [string] $Edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    [string] $EdgeProfile = (Join-Path $env:TEMP 'midi-sequencer-comps-edge-profile')
)

$ErrorActionPreference = 'Stop'
$shots = Join-Path $PSScriptRoot 'shots'
New-Item -ItemType Directory -Path $shots -Force | Out-Null

foreach ($file in Get-ChildItem $PSScriptRoot -Filter '*.html' | Sort-Object Name) {
    if ($Page -and -not $file.BaseName.StartsWith($Page)) {
        continue
    }

    $runs = @('')
    $variants = [regex]::Match((Get-Content $file.FullName -Raw), '<meta name="variants" content="([^"]*)">')

    if ($variants.Success) {
        $runs += $variants.Groups[1].Value -split '\s*,\s*' | Where-Object { $_ }
    }

    foreach ($theme in $runs) {
    $url = 'file:///' + ($file.FullName -replace '\\', '/')
    $themeQuery = if ($theme) { "theme=$theme" } else { '' }
    $checkUrl = $url + '?check' + $(if ($themeQuery) { "&$themeQuery" } else { '' })

    $dom = & $Edge --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=1 `
        "--user-data-dir=$EdgeProfile" '--window-size=1800,1400' --virtual-time-budget=3000 --dump-dom $checkUrl 2>$null | Out-String

    $match = [regex]::Match($dom, '<pre id="report">(.*?)</pre>', 'Singleline')
    $width = 1400
    $height = 1000

    if ($match.Success) {
        $report = [System.Net.WebUtility]::HtmlDecode($match.Groups[1].Value) | ConvertFrom-Json
        $width = [int]$report.page[0]
        $height = [int]$report.page[1]
    }

    $png = Join-Path $shots ($file.BaseName + $(if ($theme) { "-$theme" } else { '' }) + '.png')

    if (Test-Path $png) {
        Remove-Item $png
    }

    # The size is one quoted argument: a bare 1400,1000 would be read as a PowerShell array.
    $windowSize = '--window-size={0},{1}' -f $width, $height
    $shotUrl = $url + $(if ($themeQuery) { "?$themeQuery" } else { '' })

    & $Edge --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=1 `
        "--user-data-dir=$EdgeProfile" $windowSize "--screenshot=$png" --virtual-time-budget=3000 $shotUrl 2>$null | Out-Null

    Write-Host ("{0,-28} {1}x{2} {3}" -f (Split-Path $png -Leaf), $width, $height, $(if (Test-Path $png) { 'captured' } else { 'NOT captured' }))
    }
}
