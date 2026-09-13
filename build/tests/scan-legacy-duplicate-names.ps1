# Read-only scan for historically assigned duplicate-device MIDI/audio names.
# Looking for the shape Windows used before 2026 for a second device of the same model.

$pattern = '^\s*\d+\s*-\s*\S'

Write-Output "=== MediaCategories (KS filter/pin friendly names) ==="
$mc = 'HKLM:\SYSTEM\CurrentControlSet\Control\MediaCategories'
if (Test-Path $mc)
{
    $hits = @()
    foreach ($k in Get-ChildItem $mc -ErrorAction SilentlyContinue)
    {
        $n = (Get-ItemProperty $k.PSPath -Name 'Name' -ErrorAction SilentlyContinue).Name
        if ($n -and $n -match $pattern) { $hits += $n }
    }
    if ($hits.Count -gt 0) { $hits | ForEach-Object { "  MATCH: '$_'" } } else { "  no numeric-prefixed names" }
}
else { "  key not present" }

Write-Output ""
Write-Output "=== USB device FriendlyName / DeviceDesc with a numeric prefix ==="
$found = @()
foreach ($d in Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Enum\USB' -Recurse -Depth 2 -ErrorAction SilentlyContinue)
{
    $p = Get-ItemProperty $d.PSPath -ErrorAction SilentlyContinue
    foreach ($v in 'FriendlyName', 'DeviceDesc')
    {
        if ($p.$v -and $p.$v -match $pattern) { $found += "$v = '$($p.$v)'" }
    }
}
if ($found.Count -gt 0) { $found | Select-Object -Unique | ForEach-Object { "  $_" } } else { "  none" }

Write-Output ""
Write-Output "=== every MIDI-ish name currently under MediaCategories (for shape reference) ==="
if (Test-Path $mc)
{
    $names = @()
    foreach ($k in Get-ChildItem $mc -ErrorAction SilentlyContinue)
    {
        $n = (Get-ItemProperty $k.PSPath -Name 'Name' -ErrorAction SilentlyContinue).Name
        if ($n -and $n -match 'MIDI|ESI|Steinberg|SoftStep|Express|Push|Iridium|Moog|UM-ONE|KOMPLETE') { $names += $n }
    }
    $names | Select-Object -Unique | Sort-Object | ForEach-Object { "  $_" }
}
