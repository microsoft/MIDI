# PROTOTYPE. Style studies for the Windows MIDI Sequencer: the main window comp (..\1-main.html)
# in six looks, three light and three dark, each a stylesheet over ..\seq.css. Makes one page per
# study from the comp, then captures each to shots\<name>.png with headless Edge. Nothing is shown
# on screen.
#
#   pwsh -File make.ps1                     every study and variation
#   pwsh -File make.ps1 -Name clay          one study
#   pwsh -File make.ps1 -Name 'aluminum-*'  Aluminum's variations (a wildcard)
#   pwsh -File make.ps1 -Name 'porcelain-2*' Porcelain's second pass
#   pwsh -File make.ps1 -NoCapture          pages only

param(
    [string] $Name = '',
    [switch] $NoCapture,
    [string] $Edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    [string] $EdgeProfile = (Join-Path $env:TEMP 'midi-sequencer-studies-edge-profile')
)

$ErrorActionPreference = 'Stop'

# Name, light or dark, title, and one line on the idea. The order is the gallery's.
$studies = @(
    @{ Name = 'porcelain'; Light = $true;  Title = 'Porcelain'; Idea = 'One warm gray material. Every control is a soft key pressed out of it, lit from the top left, so shadows define the buttons. Color only as small caps, the way a synth marks its encoders.' }
    @{ Name = 'aluminum';  Light = $true;  Title = 'Aluminum'; Idea = 'Matte machined plates split by engraved grooves, under a dark header strip. What you touch is a small graphite object with a short, crisp shadow; color lives in rings and LEDs.' }
    @{ Name = 'vellum';    Light = $true;  Title = 'Vellum'; Idea = 'Warm paper, ink hairlines, a serif for names, and one vermilion accent. Clips are watercolor washes with ink notes. Only what floats, like the clip editor, casts a long soft shadow.' }
    @{ Name = 'clay';      Light = $false; Title = 'Clay'; Idea = 'One violet charcoal material, pressed and carved, with almost no lines. Track color shows up only as thin glowing strips and arcs, and quiet lowercase labels keep it calm.' }
    @{ Name = 'lantern';   Light = $false; Title = 'Lantern'; Idea = 'Paper lanterns in a dark room: one warm pool of light, clips as lit paper with their notes in ink, launcher clips that stay dark until they play, and keys that light up from inside.' }
    @{ Name = 'ink-gold';  Light = $false; Title = 'Ink and Gold'; Idea = 'Cinematic near-black with grain, drawn in hairlines and white pucks. One warm gold marks whatever is active, so it always stands out; tracks differ by shade, not by rainbow.' }
)

# Aluminum, further (aluminum-variations.html): dark, and the window's color. Each layers stylesheets
# over aluminum.css, which they leave as it is. Tint is the window color: the Sequencer's aqua and
# MIDI Clock's plum, as picked on this PC. Any of these pages takes ?tint=RRGGBB, and &anodize=0 for
# plain metal.
$variants = @(
    @{ Name = 'aluminum-aqua'; Light = $true; Css = @('aluminum', 'aluminum-tint'); Classes = 'aluminum tinted'; Tint = '#031E24'
       Title = 'Aluminum, in your aqua'; Idea = 'The window color you picked for the Sequencer anodizes the metal: the strips deep, the plates faintly, the readouts backlit, and the LEDs and selection lit in it. Clips, keys, pads, and track colors keep their own.' }
    @{ Name = 'aluminum-plum'; Light = $true; Css = @('aluminum', 'aluminum-tint'); Classes = 'aluminum tinted'; Tint = '#33192F'
       Title = 'Aluminum, in plum'; Idea = 'The plum you picked for MIDI Clock, the same way. One color works in light and dark: it brings its hue, and the theme brings the lightness.' }
    @{ Name = 'aluminum-dark'; Light = $false; Css = @('aluminum', 'aluminum-dark', 'aluminum-tint'); Classes = 'aluminum aluminum-dark'
       Title = 'Aluminum, dark'; Idea = 'The same plates, grooves, and graphite keys, anodized a dark gray that sits with a dark mode desktop: mid grays, not black. The timeline is a step darker than the headers, so clips stand forward.' }
    @{ Name = 'aluminum-dark-aqua'; Light = $false; Css = @('aluminum', 'aluminum-dark', 'aluminum-tint'); Classes = 'aluminum aluminum-dark tinted'; Tint = '#031E24'
       Title = 'Aluminum, dark, in your aqua'; Idea = 'The strips are your color at about the depth you picked it, and the plates a dark anodized aqua. The readouts are backlit in it, and the LEDs and selection lit in it.' }
    @{ Name = 'aluminum-dark-plum'; Light = $false; Css = @('aluminum', 'aluminum-dark', 'aluminum-tint'); Classes = 'aluminum aluminum-dark tinted'; Tint = '#33192F'
       Title = 'Aluminum, dark, in plum'; Idea = 'The same in MIDI Clock''s plum: deep plum strips, dark aubergine plates, and pink-violet readouts and lights.' }

    # Porcelain, second pass (porcelain-2.html, compared in porcelain-second-pass.html): the chrome
    # flat, with the timeline as one well or none.
    @{ Name = 'porcelain-2'; Light = $true; Css = @('porcelain', 'porcelain-2'); Classes = 'porcelain porcelain-2 one-well'
       Title = 'Porcelain, second pass'; Idea = 'The chrome is one flat surface: the transport and the clip editor are part of the window, and the track headers sit flush on it. What''s raised is what you touch and what you made: keys, clips, and pads. The timeline is one well for all the tracks.' }
    @{ Name = 'porcelain-2-no-well'; Light = $true; Css = @('porcelain', 'porcelain-2', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well'
       Title = 'Porcelain, second pass, no well'; Idea = 'The chrome is one flat surface and the track headers sit flush on it. The timeline sits on the surface too, on a field a step darker, like the piano roll''s, with its grid engraved in it, so only the clips stand out.' }

    # Porcelain, approved (no well): dark, and the window's color (porcelain-variations.html). It
    # glazes the one material where Aluminum anodizes its strips; ?glaze=0 for plain porcelain.
    @{ Name = 'porcelain-2-aqua'; Light = $true; Css = @('porcelain', 'porcelain-2', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well tinted'; Tint = '#031E24'
       Title = 'Porcelain, in your aqua'; Idea = 'The window color you picked for the Sequencer glazes the porcelain: the body, keys, wells, and clip faces take a faint aqua, the LEDs and selection light up in it, and the readouts are printed in it. Track caps and notes keep their own colors.' }
    @{ Name = 'porcelain-2-plum'; Light = $true; Css = @('porcelain', 'porcelain-2', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well tinted'; Tint = '#33192F'
       Title = 'Porcelain, in plum'; Idea = 'The plum you picked for MIDI Clock, the same way. One color works in light and dark: it brings its hue, and the theme brings the lightness.' }
    @{ Name = 'porcelain-2-dark'; Light = $false; Css = @('porcelain', 'porcelain-2', 'porcelain-dark', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well porcelain-dark'
       Title = 'Porcelain, dark'; Idea = 'The second pass in a dark, warm gray porcelain at a dark mode window''s depth: mid grays, not black. Deep shadows below and faint highlights above, so the keys are still defined by their shadows.' }
    @{ Name = 'porcelain-2-dark-aqua'; Light = $false; Css = @('porcelain', 'porcelain-2', 'porcelain-dark', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well porcelain-dark tinted'; Tint = '#031E24'
       Title = 'Porcelain, dark, in your aqua'; Idea = 'A dark slate glaze in your aqua, with the LEDs, the selection, and the readouts lit in it.' }
    @{ Name = 'porcelain-2-dark-plum'; Light = $false; Css = @('porcelain', 'porcelain-2', 'porcelain-dark', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well porcelain-dark tinted'; Tint = '#33192F'
       Title = 'Porcelain, dark, in plum'; Idea = 'The same in MIDI Clock''s plum: a dark plum glaze, with pink-violet lights and readouts.' }

    # Softer shadows for dark Porcelain, tried first on dark aqua (porcelain-shadows.html).
    @{ Name = 'porcelain-2-dark-aqua-soft-a'; Light = $false; Css = @('porcelain', 'porcelain-2', 'porcelain-dark', 'porcelain-soft', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well porcelain-dark tinted soft-a'; Tint = '#031E24'
       Title = 'Porcelain, dark, in your aqua: A, lighter'; Idea = 'The same shadows at about half the strength.' }
    @{ Name = 'porcelain-2-dark-aqua-soft-b'; Light = $false; Css = @('porcelain', 'porcelain-2', 'porcelain-dark', 'porcelain-soft', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well porcelain-dark tinted soft-b'; Tint = '#031E24'
       Title = 'Porcelain, dark, in your aqua: B, lighter and more diffuse'; Idea = 'About half the strength and half again the blur, a little closer to the surface, with a faint contact shadow so the edges stay crisp.' }
    @{ Name = 'porcelain-2-dark-aqua-soft-c'; Light = $false; Css = @('porcelain', 'porcelain-2', 'porcelain-dark', 'porcelain-soft', 'porcelain-tint'); Classes = 'porcelain porcelain-2 no-well porcelain-dark tinted soft-c'; Tint = '#031E24'
       Title = 'Porcelain, dark, in your aqua: C, in the glaze'; Idea = 'As B, with the shadows a deep shade of your aqua instead of black, and the light from the top left a pale tint of it.' }

    # Vellum, second pass (vellum-variations.html): Candara instead of Sitka, dark, and the window's
    # color, which dyes the paper. Layers over vellum.css; ?tint=RRGGBB and &dye=0 on any of them.
    @{ Name = 'vellum-2'; Light = $true; Css = @('vellum', 'vellum-2', 'vellum-tint'); Classes = 'vellum'
       Title = 'Vellum, in Candara'; Idea = 'Paper and ink, with Candara, a humanist sans that ships with Windows, where Sitka was: names, readouts, scenes, bar numbers, clip captions, and section marks.' }
    @{ Name = 'vellum-2-aqua'; Light = $true; Css = @('vellum', 'vellum-2', 'vellum-tint'); Classes = 'vellum tinted'; Tint = '#031E24'
       Title = 'Vellum, in your aqua'; Idea = 'Your window color dyes the paper, the way vellum takes a tint, and the ink leans toward it. Vermilion stays the one accent, and the washes keep their track colors.' }
    @{ Name = 'vellum-2-plum'; Light = $true; Css = @('vellum', 'vellum-2', 'vellum-tint'); Classes = 'vellum tinted'; Tint = '#33192F'
       Title = 'Vellum, in plum'; Idea = 'The plum you picked for MIDI Clock, the same way.' }
    @{ Name = 'vellum-2-dark'; Light = $false; Css = @('vellum', 'vellum-2', 'vellum-dark', 'vellum-tint'); Classes = 'vellum vellum-dark'
       Title = 'Vellum, dark'; Idea = 'The same page at night: warm charcoal paper, cream ink and hairlines, a brighter vermilion, and washes in track colors lifted to show on the dark paper.' }
    @{ Name = 'vellum-2-dark-aqua'; Light = $false; Css = @('vellum', 'vellum-2', 'vellum-dark', 'vellum-tint'); Classes = 'vellum vellum-dark tinted'; Tint = '#031E24'
       Title = 'Vellum, dark, in your aqua'; Idea = 'Dark paper dyed in your aqua, with the ink leaning toward it.' }
    @{ Name = 'vellum-2-dark-plum'; Light = $false; Css = @('vellum', 'vellum-2', 'vellum-dark', 'vellum-tint'); Classes = 'vellum vellum-dark tinted'; Tint = '#33192F'
       Title = 'Vellum, dark, in plum'; Idea = 'Dark paper dyed in plum.' }
)

# Lets a page try any window color: ?tint=RRGGBB, with &anodize=0 (Aluminum) or &glaze=0
# (Porcelain) to keep the material plain.
$tryColor = "<script>(() => { const q = new URLSearchParams(location.search); const root = document.documentElement; const tint = q.get('tint'); if (tint) { root.style.setProperty('--tint', '#' + tint.replace(/^#/, '')); root.classList.add('tinted'); } for (const knob of ['anodize', 'glaze', 'dye']) { if (q.has(knob)) { root.style.setProperty('--' + knob, q.get(knob)); } } })();</script>`n"

$here = $PSScriptRoot
$comp = Get-Content (Join-Path $here '..\1-main.html') -Raw
$shots = Join-Path $here 'shots'
New-Item -ItemType Directory -Path $shots -Force | Out-Null

foreach ($study in $studies + $variants) {
    if ($Name -and $study.Name -notlike $Name) {
        continue
    }

    # A variation names its layers and classes; a study is one stylesheet of its own name.
    $variant = $null -ne $study.Css
    $sheets = if ($variant) { $study.Css } else { @($study.Name) }
    $classes = if ($variant) { $study.Classes } else { $study.Name }

    $page = $comp
    $page = $page.Replace('<html lang="en-us">', ('<html lang="en-us" class="study {0}">' -f $classes))
    $page = $page.Replace('<meta name="variants" content="light">', '')
    $page = $page.Replace('<title>Windows MIDI Sequencer - 1 - main window</title>', ('<title>Style study: {0}</title>' -f $study.Title))

    # A light study is light when opened directly too: seq.js reads the theme from the address. A
    # variation keeps the rest of the address, which can carry a color to try.
    $light = if (-not $study.Light) { '' }
        elseif ($variant) { "<script>if (!location.search.includes('theme=')) { const q = new URLSearchParams(location.search); q.set('theme', 'light'); history.replaceState(null, '', '?' + q); }</script>`n" }
        else { "<script>if (!location.search.includes('theme=')) { history.replaceState(null, '', '?theme=light'); }</script>`n" }

    $tint = if ($study.Tint) { ('<style>html.study {{ --tint: {0}; }}</style>' -f $study.Tint) + "`n" } else { '' }
    $try = if ($sheets -contains 'aluminum-tint' -or $sheets -contains 'porcelain-tint' -or $sheets -contains 'vellum-tint') { $tryColor } else { '' }
    $sheetLinks = ($sheets | ForEach-Object { '<link rel="stylesheet" href="{0}.css">' -f $_ }) -join "`n"
    $links = '<link rel="stylesheet" href="../seq.css">' + "`n" + '<link rel="stylesheet" href="studies.css">' + "`n" + $sheetLinks + "`n" + $tint + $try + $light
    $page = $page.Replace('<link rel="stylesheet" href="seq.css">', $links)
    $page = $page.Replace('<script src="seq.js" defer></script>', '<script src="../seq.js" defer></script>')

    # The study's own caption in place of the comp's, and no callouts.
    $page = [regex]::Replace($page, '<div class="caption-above">.*?</div>', ('<div class="caption-above"><b>{0}.</b> {1}</div>' -f $study.Title, $study.Idea), 'Singleline')
    $page = [regex]::Replace($page, '<ol class="callouts">.*?</ol>', '', 'Singleline')

    $html = Join-Path $here ($study.Name + '.html')
    [System.IO.File]::WriteAllText($html, $page, (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("{0,-16} page written" -f $study.Name)

    if ($NoCapture) {
        continue
    }

    $png = Join-Path $shots ($study.Name + '.png')

    if (Test-Path $png) {
        Remove-Item $png
    }

    $url = 'file:///' + ($html -replace '\\', '/') + $(if ($study.Light) { '?theme=light' } else { '' })

    # A profile of its own each time, so one capture never waits on another's.
    $profileDir = Join-Path $EdgeProfile $study.Name

    $arguments = @('--headless=new', '--disable-gpu', '--hide-scrollbars', '--force-device-scale-factor=1', "--user-data-dir=$profileDir",
        '--disable-renderer-backgrounding', '--disable-background-timer-throttling', '--disable-backgrounding-occluded-windows', '--no-first-run',
        '--window-size=1668,1092', "--screenshot=$png", '--virtual-time-budget=4000', $url)

    $browser = Start-Process -FilePath $Edge -ArgumentList $arguments -PassThru -WindowStyle Hidden
    $waited = [Diagnostics.Stopwatch]::StartNew()

    # A headless page can count as in the background, and on a busy PC an idle-priority renderer
    # may wait minutes for its turn, so its processes are kept at normal priority.
    while (-not $browser.HasExited -and $waited.Elapsed.TotalSeconds -lt 120) {
        Get-CimInstance Win32_Process -Filter "Name='msedge.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($profileDir) } | ForEach-Object {
            $process = Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue

            if ($process -and $process.PriorityClass -in 'Idle', 'BelowNormal') {
                try { $process.PriorityClass = 'Normal' } catch { }
            }
        }

        Start-Sleep -Milliseconds 400
    }

    # Whatever is left of it, so the next capture starts clean.
    Get-CimInstance Win32_Process -Filter "Name='msedge.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($profileDir) } | ForEach-Object {
        try { [System.Diagnostics.Process]::GetProcessById([int]$_.ProcessId).Kill() } catch { }
    }
    Write-Host ("{0,-16} {1}" -f $study.Name, $(if (Test-Path $png) { 'captured' } else { 'NOT captured' }))
}
