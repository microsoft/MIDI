# PROTOTYPE. The timeline's own scroll bar (index.html): the main window comp (..\1-main.html) in the
# Tactile and Aluminum looks from ..\style-studies, with the bar that scroll.js adds and scroll.css
# styles. Makes one page per comp, then captures each to shots\<name>.png with headless Edge. Nothing
# is shown on screen.
#
#   pwsh -File make.ps1                    every comp
#   pwsh -File make.ps1 -Name 'tactile-*'  some of them (a wildcard)
#   pwsh -File make.ps1 -NoCapture         pages only

param(
    [string] $Name = '',
    [switch] $NoCapture,
    [string] $Edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    [string] $EdgeProfile = (Join-Path $env:TEMP 'midi-sequencer-scroll-edge-profile')
)

$ErrorActionPreference = 'Stop'

# The looks, as the style studies layer them.
$tactile = @{ Light = $true; Css = @('porcelain', 'porcelain-2', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well' }
$aluminum = @{ Light = $true; Css = @('aluminum'); Classes = 'aluminum' }
$tactileDarkAqua = @{ Light = $false; Css = @('porcelain', 'porcelain-2', 'porcelain-dark', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well porcelain-dark tinted'; Tint = '#031E24' }
$aluminumDarkAqua = @{ Light = $false; Css = @('aluminum', 'aluminum-dark', 'aluminum-tint'); Classes = 'aluminum aluminum-dark tinted'; Tint = '#031E24' }

# Each comp: a look, the kind of bar, where it is, the bars in view, and the zoom controls.
$comps = @(
    @{ Name = 'tactile-overview'; Look = $tactile; Bar = 'overview'; Place = 'bottom'; View = '5 17'; Zoom = 'buttons'
       Title = 'Tactile: the overview, under the timeline'; Idea = 'The whole sequence in a well the color of the field: a line for each track where its clips are, the sections by name, the loop, the playhead, and under the lines, where the selected clip''s linked copies play. The view is a raised, clear frame: drag it to move, or drag a grip to zoom. Zoom keys at the right; the switch at the left changes to the plain bar.' }
    @{ Name = 'aluminum-overview'; Look = $aluminum; Bar = 'overview'; Place = 'bottom'; View = '5 17'; Zoom = 'slider'
       Title = 'Aluminum: the overview, under the timeline'; Idea = 'The same overview as a dark display, like the readouts, with a metal bracket on the view and a knurled tab at each end to zoom. Here the zoom control is a slider between a small and a large triangle, as in Studio One.' }
    @{ Name = 'tactile-plain'; Look = $tactile; Bar = 'plain'; Place = 'bottom'; View = '5 17'; Zoom = 'slider'; Hint = $true
       Title = 'Tactile: the plain bar'; Idea = 'A groove with a porcelain key as long as the view. Drag the key to move, or drag either end to zoom. Only the playhead marks the groove. Here the zoom control is the slider with triangles.' }
    @{ Name = 'aluminum-plain'; Look = $aluminum; Bar = 'plain'; Place = 'bottom'; View = '5 17'; Zoom = 'buttons'; Hint = $true
       Title = 'Aluminum: the plain bar'; Idea = 'An engraved slot with a brushed thumb and knurled ends. Zoom keys at the right, as in Cubase: zoom out, zoom in, and zoom to fit the sequence.' }
    @{ Name = 'tactile-top'; Look = $tactile; Bar = 'overview'; Place = 'top'; View = '5 17'; Zoom = 'toolbar'
       Title = 'Tactile: the overview above the ruler'; Idea = 'Set to the top, the overview lines up with the ruler and the timeline under it, and the zoom moves into the track list''s toolbar, as in Logic Pro.' }
    @{ Name = 'aluminum-top'; Look = $aluminum; Bar = 'overview'; Place = 'top'; View = '5 17'; Zoom = 'toolbar'
       Title = 'Aluminum: the overview above the ruler'; Idea = 'The display above the ruler, under the transport''s readouts, with the zoom in the track list''s toolbar.' }
    @{ Name = 'tactile-whole'; Look = $tactile; Bar = 'overview'; Place = 'bottom'; View = '1 27'; Zoom = 'buttons'
       Title = 'Zoomed out to the whole sequence'; Idea = 'The view fills the overview. The grid shows each bar faintly and every fourth bar strongly, and the ruler numbers every other bar.' }
    @{ Name = 'tactile-twelve'; Look = $tactile; Bar = 'overview'; Place = 'bottom'; View = '5 17'; Zoom = 'buttons'
       Title = 'Twelve bars'; Idea = 'The frame covers about half the overview. The grid shows bars and beats, and the ruler numbers every bar.' }
    @{ Name = 'tactile-close'; Look = $tactile; Bar = 'overview'; Place = 'bottom'; View = '9 12.5'; Zoom = 'buttons'
       Title = 'Zoomed in to bar 9'; Idea = 'The view is a narrow frame on the overview. The grid shows beats and sixteenths, the ruler numbers the beats, and the clips spread out.' }
    @{ Name = 'tactile-dark-aqua'; Look = $tactileDarkAqua; Bar = 'overview'; Place = 'bottom'; View = '5 17'; Zoom = 'buttons'
       Title = 'Tactile, dark, in your aqua'; Idea = 'The overview''s well and frame in the dark glaze, with the loop and the linked copies lit in your color.' }
    @{ Name = 'aluminum-dark-aqua'; Look = $aluminumDarkAqua; Bar = 'overview'; Place = 'bottom'; View = '5 17'; Zoom = 'slider'
       Title = 'Aluminum, dark, in your aqua'; Idea = 'The display backlit in your aqua, like the readouts, with the loop and the linked copies lit in it.' }
    @{ Name = 'tactile-menu'; Look = $tactile; Bar = 'overview'; Place = 'bottom'; View = '5 17'; Zoom = 'buttons'; Menu = $true
       Title = 'The bar''s menu'; Idea = 'Right-click the bar to switch between the overview and the plain bar, put it under the timeline or above the ruler, or zoom. The same choices are in the app''s settings.' }
)

$here = $PSScriptRoot
$comp = Get-Content (Join-Path $here '..\1-main.html') -Raw
$shots = Join-Path $here 'shots'
New-Item -ItemType Directory -Path $shots -Force | Out-Null

foreach ($c in $comps) {
    if ($Name -and $c.Name -notlike $Name) {
        continue
    }

    $look = $c.Look
    $data = 'data-hbar="{0}" data-place="{1}" data-view="{2}" data-zoom="{3}"' -f $c.Bar, $c.Place, $c.View, $c.Zoom

    if ($c.Menu) {
        $data += ' data-menu'
    }

    if ($c.Hint) {
        $data += ' data-hint'
    }

    $page = $comp
    $page = $page.Replace('<html lang="en-us">', ('<html lang="en-us" class="study {0}" {1}>' -f $look.Classes, $data))
    $page = $page.Replace('<meta name="variants" content="light">', '')
    $page = $page.Replace('<title>Windows MIDI Sequencer - 1 - main window</title>', ('<title>Timeline scroll bar: {0}</title>' -f $c.Title))

    $light = if ($look.Light) { "<script>if (!location.search.includes('theme=')) { history.replaceState(null, '', '?theme=light'); }</script>`n" } else { '' }
    $tint = if ($look.Tint) { ('<style>html.study {{ --tint: {0}; }}</style>' -f $look.Tint) + "`n" } else { '' }
    $sheetLinks = ($look.Css | ForEach-Object { '<link rel="stylesheet" href="../style-studies/{0}.css">' -f $_ }) -join "`n"
    $links = '<link rel="stylesheet" href="../seq.css">' + "`n" + '<link rel="stylesheet" href="../style-studies/studies.css">' + "`n" + $sheetLinks + "`n" +
        '<link rel="stylesheet" href="scroll.css">' + "`n" + $tint + $light
    $page = $page.Replace('<link rel="stylesheet" href="seq.css">', $links)

    # scroll.js first: it sets the timeline's scale before seq.js draws it.
    $page = $page.Replace('<script src="seq.js" defer></script>', '<script src="scroll.js" defer></script>' + "`n" + '<script src="../seq.js" defer></script>')

    $page = [regex]::Replace($page, '<div class="caption-above">.*?</div>', ('<div class="caption-above"><b>{0}.</b> {1}</div>' -f $c.Title, $c.Idea), 'Singleline')
    $page = [regex]::Replace($page, '<ol class="callouts">.*?</ol>', '', 'Singleline')

    $html = Join-Path $here ($c.Name + '.html')
    [System.IO.File]::WriteAllText($html, $page, (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("{0,-20} page written" -f $c.Name)

    if ($NoCapture) {
        continue
    }

    $png = Join-Path $shots ($c.Name + '.png')

    if (Test-Path $png) {
        Remove-Item $png
    }

    $url = 'file:///' + ($html -replace '\\', '/') + $(if ($look.Light) { '?theme=light' } else { '' })
    $profileDir = Join-Path $EdgeProfile $c.Name

    $arguments = @('--headless=new', '--disable-gpu', '--hide-scrollbars', '--force-device-scale-factor=1', "--user-data-dir=$profileDir",
        '--disable-renderer-backgrounding', '--disable-background-timer-throttling', '--disable-backgrounding-occluded-windows', '--no-first-run',
        '--window-size=1668,1092', "--screenshot=$png", '--virtual-time-budget=4000', $url)

    $browser = Start-Process -FilePath $Edge -ArgumentList $arguments -PassThru -WindowStyle Hidden
    $waited = [Diagnostics.Stopwatch]::StartNew()

    # A headless page can count as in the background, and on a busy PC an idle-priority renderer may
    # wait minutes for its turn, so its processes are kept at normal priority.
    while (-not $browser.HasExited -and $waited.Elapsed.TotalSeconds -lt 120) {
        Get-CimInstance Win32_Process -Filter "Name='msedge.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($profileDir) } | ForEach-Object {
            $process = Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue

            if ($process -and $process.PriorityClass -in 'Idle', 'BelowNormal') {
                try { $process.PriorityClass = 'Normal' } catch { }
            }
        }

        Start-Sleep -Milliseconds 400
    }

    Get-CimInstance Win32_Process -Filter "Name='msedge.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($profileDir) } | ForEach-Object {
        try { [System.Diagnostics.Process]::GetProcessById([int]$_.ProcessId).Kill() } catch { }
    }

    Write-Host ("{0,-20} {1}" -f $c.Name, $(if (Test-Path $png) { 'captured' } else { 'NOT captured' }))
}
