# PROTOTYPE. Reports layout problems in each comp (text cut off, overlaps, content past its scroll
# area, flyouts or dialogs outside the window) without taking a picture. Each page measures itself
# when it is opened with ?check. Variants listed in <meta name="variants"> are checked too.
#
#   pwsh -File check.ps1            every page
#   pwsh -File check.ps1 -Page 2    pages whose name starts with 2

param(
    [string] $Page = '',
    [string] $Edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    [string] $EdgeProfile = (Join-Path $env:TEMP 'midi-settings-comps-edge-profile')
)

$ErrorActionPreference = 'Stop'
$total = 0

foreach ($file in Get-ChildItem $PSScriptRoot -Filter '*.html' | Sort-Object Name) {
    if ($Page -and -not $file.BaseName.StartsWith($Page)) {
        continue
    }

    $runs = @('')
    $variants = [regex]::Match((Get-Content $file.FullName -Raw), '<meta name="variants" content="([^"]*)">')

    if ($variants.Success) {
        $runs += $variants.Groups[1].Value -split '\s*,\s*' | Where-Object { $_ }
    }

    foreach ($state in $runs) {
        $url = 'file:///' + ($file.FullName -replace '\\', '/') + '?check' + $(if ($state) { "&state=$state" } else { '' })

        $dom = & $Edge --headless=new --disable-gpu --hide-scrollbars --disable-lcd-text --force-device-scale-factor=1 `
            "--user-data-dir=$EdgeProfile" '--window-size=1800,1400' --virtual-time-budget=3000 --dump-dom $url 2>$null | Out-String

        $name = $file.BaseName + $(if ($state) { "-$state" } else { '' })
        $match = [regex]::Match($dom, '<pre id="report"[^>]*>(.*?)</pre>', 'Singleline')

        if (-not $match.Success) {
            Write-Host ("{0,-34} no report (page has no cfg.js, or it failed)" -f $name)
            continue
        }

        $report = [System.Net.WebUtility]::HtmlDecode($match.Groups[1].Value) | ConvertFrom-Json
        Write-Host ("{0,-34} page {1}x{2}, {3} problem(s)" -f $name, $report.page[0], $report.page[1], $report.problems.Count)
        $total += $report.problems.Count

        foreach ($problem in $report.problems) {
            Write-Host "    $problem"
        }
    }
}

Write-Host "total problems: $total"
