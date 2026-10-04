# PROTOTYPE. Reports layout problems in each comp (overlaps, text cut off, things outside the
# window) without taking a picture. Each page measures itself when it is opened with ?check.
#
#   pwsh -File check.ps1            every page
#   pwsh -File check.ps1 -Page 2    pages whose name starts with 2

param(
    [string] $Page = '',
    [string] $Edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    [string] $EdgeProfile = (Join-Path $env:TEMP 'midi-patchbay-comps-edge-profile')
)

$ErrorActionPreference = 'Stop'

foreach ($file in Get-ChildItem $PSScriptRoot -Filter '*.html' | Sort-Object Name) {
    if ($Page -and -not $file.BaseName.StartsWith($Page)) {
        continue
    }

    $url = 'file:///' + ($file.FullName -replace '\\', '/') + '?check'

    $dom = & $Edge --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=1 `
        "--user-data-dir=$EdgeProfile" '--window-size=1800,1400' --virtual-time-budget=3000 --dump-dom $url 2>$null | Out-String

    $match = [regex]::Match($dom, '<pre id="report">(.*?)</pre>', 'Singleline')

    if (-not $match.Success) {
        Write-Host ("{0,-24} no report (page has no mock2.js, or it failed)" -f $file.Name)
        continue
    }

    $report = [System.Net.WebUtility]::HtmlDecode($match.Groups[1].Value) | ConvertFrom-Json
    Write-Host ("{0,-24} page {1}x{2}, {3} problem(s)" -f $file.Name, $report.page[0], $report.page[1], $report.problems.Count)

    foreach ($problem in $report.problems) {
        Write-Host "    $problem"
    }
}
