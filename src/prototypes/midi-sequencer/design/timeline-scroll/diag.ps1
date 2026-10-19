param([string[]] $Names)
# Builds the named pages in headless Edge with ?diag and prints what scroll.js measured (diagnose()
# in scroll.js), so a layout can be checked without a screenshot.
$edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe"
# pwsh -File passes a list as one string, so commas split it here.
foreach ($n in ($Names | ForEach-Object { $_ -split ',' } | Where-Object { $_ })) {
    $html = Join-Path $PSScriptRoot "$n.html"
    $light = (Get-Content $html -Raw).Contains("history.replaceState(null, '', '?theme=light')")
    $url = 'file:///' + ($html -replace '\\', '/') + '?diag' + $(if ($light) { '&theme=light' } else { '' })
    $profileDir = Join-Path $env:TEMP "midi-scroll-diag\$n"
    $outFile = Join-Path $env:TEMP "midi-scroll-diag-$n.html"
    $errFile = Join-Path $env:TEMP "midi-scroll-diag-$n.log"
    $p = Start-Process -FilePath $edge -ArgumentList @('--headless=new', '--disable-gpu', '--hide-scrollbars', '--force-device-scale-factor=1',
        "--user-data-dir=$profileDir", '--no-first-run', '--allow-file-access-from-files', '--window-size=1668,1092', '--virtual-time-budget=6000',
        '--enable-logging=stderr', '--v=0', '--dump-dom', $url) -PassThru -WindowStyle Hidden -RedirectStandardOutput $outFile -RedirectStandardError $errFile
    if (-not $p.WaitForExit(90000)) { try { $p.Kill() } catch { } }
    Get-CimInstance Win32_Process -Filter "Name='msedge.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($profileDir) } | ForEach-Object {
        try { [System.Diagnostics.Process]::GetProcessById([int]$_.ProcessId).Kill() } catch { }
    }
    $dom = if (Test-Path $outFile) { Get-Content $outFile -Raw } else { '' }
    $m = [regex]::Match($dom, '<pre id="hb-diag">(.*?)</pre>', 'Singleline')
    "== $n"
    if ($m.Success) { [System.Net.WebUtility]::HtmlDecode($m.Groups[1].Value) }
    else {
        '  (no diagnostics: the bar was not built)'
        if (Test-Path $errFile) { Get-Content $errFile | Where-Object { $_ -match 'CONSOLE|Uncaught' } | Select-Object -First 8 }
    }
    Remove-Item $outFile, $errFile -ErrorAction SilentlyContinue
}
