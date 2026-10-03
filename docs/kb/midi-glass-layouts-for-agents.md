---
layout: kb
title: MIDI Glass layout files, a guide for AI agents
audience: everyone
description: How an AI agent or an online AI chat designs a Windows MIDI Glass layout for someone. What to ask, the .midilayout file format, using a built-in theme, showing the customer a mockup, and getting the file into MIDI Glass.
categories:
  - Developer Guidance
---

Windows MIDI Glass ("MIDI Glass") is the Windows MIDI Services app for building your own touch control surface. You put knobs, faders, pads, buttons, and keys on a page, choose what MIDI each one sends, and play it with a finger, a pen, or a mouse. Each design is called a **layout**, and each layout is one `.midilayout` file.

This article is written for AI agents and online AI chats that build layouts for people. It's also for anyone who wants to write or check a layout file by hand. If you're asking an AI to build a layout for you, give it the link to this article and ask it to read the whole page before it starts.

> **MIDI Glass is a preview app.** The layout file described here is version 1. Later versions can add settings, and MIDI Glass keeps reading version 1 files when they do.

> **For AI agents:** Read this article from start to finish before you write anything. Follow [Building a layout for someone](#building-a-layout-for-someone) as your process, and use [The layout file](#the-layout-file) as your reference. You usually can't see MIDI Glass yourself, so the notes marked **For agents** point out mistakes that load without an error and give the customer the wrong layout.

The rules that matter most:

1. **Ask before you build.** Get the device names, the manual, the screen, and what goes on the page. Don't guess controller numbers.
2. **Use a built-in theme, by its exact name.** Don't invent theme settings.
3. **Count groups and channels from 0 in the file.** People, manuals, and MIDI Glass's own screens count them from 1.
4. **Name each device the way Windows shows it, and match it by name.** You can't know a device's ID.
5. **Write strict JSON.** No comments, no comma after the last item, and no hexadecimal numbers.
6. **Say what MIDI Glass can't do** before the customer finds out. See [What MIDI Glass can't do](#what-midi-glass-cant-do).
7. **Draw a mockup from your file** and get a yes before you hand it over.

On this page:

- [What a layout is](#what-a-layout-is)
- [Building a layout for someone](#building-a-layout-for-someone)
- [Where layout files go, and how to bring one in](#where-layout-files-go-and-how-to-bring-one-in)
- [What MIDI Glass can't do](#what-midi-glass-cant-do)
- [The layout file](#the-layout-file)
- [Mistakes that are easy to miss](#mistakes-that-are-easy-to-miss)

## What a layout is

- A layout has one or more **pages** of controls. A page has a fixed size in pixels. When the layout runs, the page is scaled to fit the window or the screen without being stretched, so it keeps its shape.
- Each **control** has a position, a size, a name, and a list of **messages**: rows that say what the control sends, when it sends it, and where.
- A layout has its own **device table**. A message names a device from that table, never the hardware itself. Point that name at different hardware and every control that uses it follows.
- A layout names a **theme**, which decides how everything looks. A control doesn't store a color. It stores one of the theme's six colors, called **slots**, so a new theme recolors everything at once and the colors keep their meaning.
- A layout can also have **sequences**: lists of steps, such as a bank select, a program change, and a pause, that a button runs.

## Building a layout for someone

### 1. Ask first

Ask these before you write anything. Put them in one message, in plain words, and offer a sensible answer for each so the customer can just say yes.

If the customer pasted a prompt from **Ask an AI assistant…** in MIDI Glass, it already lists the MIDI devices and the themes on their PC. Use those names exactly. You still need to ask which device the layout is for.

| Ask | Why it matters |
| --- | --- |
| What will the layout control? Get each device's name exactly as Windows shows it, for example in MIDI Settings or in the **Send to** list in MIDI Glass's **New layout** dialog. | The file finds devices by name. A name that's only close doesn't match. |
| Is it hardware, a plug-in, or an app like a DAW? An app on the same PC is reached through a loopback, such as **Default App Loopback (A)**, with the app listening on **Default App Loopback (B)**. | You need a device that MIDI Glass can send to. |
| Do they have the device's manual or its MIDI implementation chart? Ask for it, or for a link to it. | The same name means different numbers on different gear. Don't guess. |
| Does the device use MIDI 1.0 or MIDI 2.0? | It changes how you write values that are codes, like pad colors. Most hardware is MIDI 1.0. |
| What screen will it run on, and which way up? | It decides the page size. |
| Will they use touch, a pen, or a mouse? | Fingers need bigger controls. |
| Where will they play it: a dark stage, a lit studio, a desk by a window? | It decides the theme. |
| What should be on the page, how should it be grouped, and what matters most? | It decides the sections, the sizes, and the order. |
| Do they need more than one page? | Each page needs page tabs to get to the others. |
| For each control: what it changes, on which channel, and whether it springs back when let go, like a pitch wheel. | |
| Should anything follow the device, like a fader the DAW moves or a light that shows notes arriving? | Those controls need to listen as well as send. |
| Do the colors already mean something to them, like drums in red? | Keep the meaning. |
| Does anyone who will use it have trouble telling colors apart, or use a screen reader? | Pick a theme that doesn't depend on color alone, and give every control a clear name. |

> **For agents:** If the customer can't give you a device name yet, use a short placeholder and tell them that MIDI Glass will show the device as missing until they pick it. Don't make up a name that looks real.

### 2. Find the messages

Take every number from the manual or a published MIDI implementation chart, and tell the customer where each one came from so they can check it.

| The manual says | Message kind in the file | Notes |
| --- | --- | --- |
| Control change (CC) 74 | `controlChange`, `number` 74 | |
| NRPN with MSB 7 and LSB 54, sent as CC 99 = 7 and CC 98 = 54 | `assignedController`, `number` 950 | The number is MSB × 128 + LSB. MIDI Glass sends one MIDI 2.0 message, and Windows turns it into CC 99, CC 98, CC 6 and CC 38 for a MIDI 1.0 device. |
| RPN 0, pitch bend sensitivity, sent as CC 101 = 0 and CC 100 = 0 | `registeredController`, `number` 0 | The same rule, with CC 101 and CC 100. |
| Note 36 | `note`, `number` 36 | Go by the note number. Manuals don't agree on note names: note 60, middle C, is C3 in some and C4 in others. MIDI Glass calls it C3, the same as the rest of Windows MIDI Services. |
| Program 1 to 128 | `programChange`, `number` 0 to 127 | The file uses the number sent on the wire, one less than most manuals print. |
| Bank select, then a program | Two `controlChange` rows (0 and 32), then a `programChange` row, all on the same trigger | Rows on the same trigger go out in the order they're listed. |
| Pitch bend | `pitchBend` | No number. The middle is 0.5. |
| Channel pressure, or aftertouch | `channelPressure` | No number. |
| A MIDI 2.0 per-note controller, such as the volume of one note | `perNoteController`, `number` is the note and `controller` is the controller | Use `assignablePerNoteController` for a controller the device defines itself. See [Per-note controllers and note attributes](#per-note-controllers-and-note-attributes). |
| A MIDI 2.0 note attribute, such as an articulation | `attributeType` and `attributeData` on a `note` row | See [Per-note controllers and note attributes](#per-note-controllers-and-note-attributes). |
| A system exclusive message | `systemExclusive`, with the bytes as hexadecimal text | `F0` and `F7` are optional. |
| Any other single message, in Universal MIDI Packet form | `rawUmp`, with one to four 32-bit words | See [Raw messages](#raw-messages). |
| A DAW set up for a Mackie Control surface | `mackieControl`, with a `function` name | See [Mackie Control functions](#mackie-control-functions). |

**MIDI 2.0 profiles.** MIDI Glass sends messages. It doesn't use MIDI-CI, so it can't turn a profile on or ask a device what it supports. Check that the device or plug-in already uses the profile, then send the messages the profile describes. Where the profile needs something MIDI Glass can't do, such as a button that changes the articulation a keyboard plays, say so and offer what it can do instead.

### 3. Pick a page size and a theme

Pick the page size closest to the shape of the customer's screen. The page scales to fit, so its shape matters more than its exact size.

| Screen | Page size in pixels |
| --- | --- |
| Tablet | 1280 × 800 |
| Full HD monitor or TV | 1920 × 1080 |
| Quad HD monitor | 2560 × 1440 |
| Surface Pro or another 3 : 2 screen | 2736 × 1824 |
| Classic 4 : 3 screen | 1024 × 768 |
| Phone-shaped, or a monitor turned on its side | 1080 × 1920 |
| Horizontal toolbar that floats over other apps | 800 × 120 |
| Vertical toolbar | 120 × 800 |
| Floating palette | 360 × 360 |

For a toolbar or a palette, also read [How to make a floating toolbar or palette in MIDI Glass]({{ site.baseurl }}/kb/midi-glass-floating-toolbars/).

**Use a built-in theme.** MIDI Glass comes with twenty-five, and they've been checked for contrast and readability. Write its exact name in `theme`, capital letters and all, and leave `themeColors` out. A name MIDI Glass doesn't know silently gives the customer Studio Dark.

The built-in themes are: **Studio Dark**, **Airy System**, **Bigwig**, **Blueprint**, **Bone**, **Cathode**, **Chicago**, **Daylight**, **Five-iSH**, **Good Form**, **Groovy**, **Groovy Dark**, **Hard Sector**, **High contrast**, **Insert Coin**, **Jove**, **Night Drive**, **Off-world Colonies**, **Soft Sector**, **Supersaw**, **Terminal Amber**, **Terminal Green**, **Tonal Dark**, **Tonal Light**, and **Visor**.

| If the customer wants | Start with |
| --- | --- |
| A busy page on a dark stage | Studio Dark |
| A bright room, or a desk by a window | Daylight, or Bone for something warm |
| Big colorful pads | Insert Coin, or Tonal Dark |
| A DAW or plug-in look: grays and one accent color | Bigwig |
| A hardware synth panel | Jove, Supersaw, or Five-iSH |
| The easiest possible reading | High contrast |
| Something retro | Cathode, Terminal Amber, Terminal Green, or Chicago |

[The shipping themes]({{ site.baseurl }}/kb/midi-glass-themes/#the-shipping-themes) describes each one, and says what it costs, such as colors that are hard to tell apart. Read the row for the theme you pick.

If the customer wants a look of their own, design a theme file as well, following [Designing a theme for someone else]({{ site.baseurl }}/kb/midi-glass-themes/#designing-a-theme-for-someone-else), and name that theme in the layout.

### 4. Lay out the page

- **Positions are page pixels.** `x` and `y` are the control's top left corner, measured from the top left of the page, with `y` growing downward. `width` and `height` are its size.
- **Use multiples of 4.** The editor snaps to 4 pixels and its grid is 8, so a layout on that grid lines up when the customer edits it.
- **Keep every control on the page.** A control that hangs off any edge doesn't show when the layout runs.
- **Start from the default sizes below,** and don't go smaller for a touch screen. They're for a 1280 × 800 page. For another page size, multiply by √(√(width² + height²) ÷ 1509), then round to a multiple of 4. That's about 1.2 for 1920 × 1080 and 1.48 for 2736 × 1824.
- **Leave room for names.** Most themes print a control's name under it, about 20 pixels tall. Jove, Supersaw, Five-iSH, Soft Sector, Hard Sector, and Good Form print it above.
- **Group related controls in a Group control,** called `panel` in the file, the way a hardware panel has sections. Leave about 32 pixels inside the top of the Group for its name, and 16 to 24 pixels on the other sides.
- **Order matters.** Controls are drawn in the order they're listed, so the first is at the back. List each Group before the controls it frames.
- **Give every control a name** in `label`, even one whose name you hide. It's what a screen reader says.
- **Number `keyboardOrder` from 1** in the order a person reads the page: section by section, left to right, top to bottom. It's the order the Tab key and a screen reader follow.
- **Use the theme's six slots to mean something,** such as one slot per section, and keep each slot's meaning across pages.
- **For more than one page,** put a page tab for every page on every page, in the same place.

| Control (name in the app) | `kind` in the file | Default size | What it is |
| --- | --- | --- | --- |
| Button | `button` | 96 × 40 | On while you hold it. |
| Toggle | `toggle` | 96 × 40 | Press for on, press again for off. |
| Pad | `pad` | 56 × 56 | A square button for drums and clips. |
| Page tab | `pageTab` | 120 × 36 | Goes to another page. |
| Switch | `switch` | 180 × 44 | Two to sixteen named positions, each sending its own message. |
| Knob | `knob` | 56 × 56 | |
| Turntable | `turntable` | 160 × 160 | A platter you push. Springs back to the middle. |
| Fader | `fader` | 40 × 180 | |
| Wheel | `wheel` | 44 × 160 | A pitch or modulation wheel. |
| Ribbon | `ribbon` | 280 × 48 | A strip you slide a finger along. |
| XY pad | `xyPad` | 240 × 240 | Two values at once. |
| Joystick | `joystick` | 180 × 180 | Two values at once, and springs back to the middle. |
| Beat clock | `beatClock` | 100 × 120 | Sends MIDI clock. |
| LFO | `lfo` | 160 × 80 | Sweeps a value by itself, in time with the layout's tempo. |
| Steps | `steps` | 320 × 96 | A step sequencer. |
| Mono keyboard | `pianoKeyboard` | 420 × 120 | Piano keys that play one note at a time. |
| Note pads | `notePads` | 428 × 164 | A grid of pads that each play a note, for chords. |
| Hex pads | `hexPads` | 456 × 160 | The same, with hexagons. |
| Meter | `meter` | 24 × 180 | Shows a value the device sends. |
| Lamp | `lamp` | 32 × 32 | Lights up when the device sends something. |
| Readout | `readout` | 120 × 40 | Shows a value the device sends. |
| Stopwatch | `timeDisplay` | 180 × 64 | Counts up from when the layout starts. |
| Text | `label` | 120 × 24 | Words on the page. |
| Image | `image` | 120 × 120 | A picture or a video. |
| Group | `panel` | 280 × 220 | A frame around controls that belong together. |
| Line | `line` | 240 × 8 | A printed line that divides the page. |

### 5. Write the file

- **JSON, saved as UTF-8.** A byte order mark is allowed, but not needed.
- **Strict JSON.** No comments, no comma after the last item in a list or an object, and no hexadecimal numbers. JSON has no `0x`.
- **Names for choices,** spelled exactly as this article shows them, capital letters included: `"controlChange"`, `"fitToScreen"`.
- **Leave out what you don't need.** Anything you leave out takes the default in [The layout file](#the-layout-file). MIDI Glass writes every setting back the next time it saves the layout.
- **Ids are text,** unique in the whole layout: every page's `id`, and every control's `id` on every page. MIDI Glass writes GUIDs, but any unique text works, such as `cutoff` or `page-mix`.

If you can run commands on the customer's PC, write the file with PowerShell 7, which finds the Documents folder even when it has been moved into OneDrive:

```powershell
$folder = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'MIDI Layouts'
New-Item -ItemType Directory -Path $folder -Force | Out-Null

$path = Join-Path $folder 'Synth basics.midilayout'
if (Test-Path -LiteralPath $path) { throw 'A layout with that name is already there. Pick another name.' }

[IO.File]::WriteAllText($path, $json, [Text.UTF8Encoding]::new($false))
```

If you're an online chat, give the customer the file as a download named after the layout, ending in `.midilayout`. If you can only show text, put the whole file in one code block and tell them how to save it: paste it into Notepad, select **File** > **Save as**, set **Save as type** to **All files**, type a name that ends in `.midilayout`, and leave **Encoding** at **UTF-8**.

> **For agents:** Don't change a layout file that's open in MIDI Glass. The app saves its own copy as the customer works, and it can write over your change. Write a new file instead.

### 6. Check it

Go through [Mistakes that are easy to miss](#mistakes-that-are-easy-to-miss) for every file. If you can run PowerShell 7, save this as `Test-MidiGlassLayout.ps1` and run `pwsh -File Test-MidiGlassLayout.ps1 -Path "<layout file>"`. It reads the file as strictly as MIDI Glass does, and lists the mistakes that load without an error.

```powershell
param([Parameter(Mandatory)][string]$Path)

$text = [IO.File]::ReadAllText($Path)
$null = [Text.Json.JsonDocument]::Parse($text)   # throws on a comment, a trailing comma, or a hexadecimal number
$layout = $text | ConvertFrom-Json
$problems = [Collections.Generic.List[string]]::new()

$themes = 'Studio Dark', 'Airy System', 'Bigwig', 'Blueprint', 'Bone', 'Cathode', 'Chicago', 'Daylight', 'Five-iSH', 'Good Form', 'Groovy', 'Groovy Dark', 'Hard Sector', 'High contrast', 'Insert Coin', 'Jove', 'Night Drive', 'Off-world Colonies', 'Soft Sector', 'Supersaw', 'Terminal Amber', 'Terminal Green', 'Tonal Dark', 'Tonal Light', 'Visor'
$controlKinds = 'knob', 'fader', 'pad', 'button', 'toggle', 'xyPad', 'meter', 'lamp', 'readout', 'label', 'image', 'pageTab', 'panel', 'joystick', 'ribbon', 'pianoKeyboard', 'beatClock', 'timeDisplay', 'lfo', 'turntable', 'wheel', 'switch', 'steps', 'line', 'notePads', 'hexPads'
$messageKinds = 'note', 'controlChange', 'programChange', 'pitchBend', 'channelPressure', 'perNoteController', 'assignablePerNoteController', 'registeredController', 'assignedController', 'systemExclusive', 'rawUmp', 'sequence', 'goToPage', 'mackieControl'
$numbered = 'note', 'controlChange', 'programChange', 'perNoteController', 'assignablePerNoteController', 'registeredController', 'assignedController'

$pageWidth = $layout.pageWidth ?? 1280
$pageHeight = $layout.pageHeight ?? 800
$devices = @($layout.devices | ForEach-Object { $_.name })
$pageIds = @($layout.pages | ForEach-Object { $_.id })
$bandIds = @($layout.pages | Where-Object { $_.sharedBand } | ForEach-Object { $_.id })
$sequences = @($layout.sequences | ForEach-Object { $_.name })
$controlIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)

if ($null -eq $layout.themeColors -and $layout.theme -cnotin $themes) { $problems.Add("'$($layout.theme)' isn't a built-in theme. Unless that theme file is installed, the customer sees Studio Dark.") }
if ($pageIds.Count -eq 0) { $problems.Add('The layout has no pages.') }
foreach ($name in ($devices | Group-Object -CaseSensitive | Where-Object Count -gt 1).Name) { $problems.Add("Two devices are named '$name'.") }
foreach ($id in ($pageIds | Group-Object -CaseSensitive | Where-Object Count -gt 1).Name) { $problems.Add("Two pages have the id '$id'.") }
foreach ($name in ($sequences | Group-Object -CaseSensitive | Where-Object Count -gt 1).Name) { $problems.Add("Two sequences are named '$name'.") }
if ($layout.tempo.kind -ceq 'followIncomingClock' -and $layout.tempo.device -cnotin $devices) { $problems.Add("The tempo follows the clock from '$($layout.tempo.device)', which isn't in devices.") }

foreach ($page in $layout.pages) {
    foreach ($c in $page.controls) {
        $where = "'$($c.label)' ($($c.id)) on page '$($page.name)'"
        if (-not $c.id -or -not $controlIds.Add($c.id)) { $problems.Add("$where needs an id of its own.") }
        if ($c.kind -cnotin $controlKinds) { $problems.Add("$where has the kind '$($c.kind)', which MIDI Glass opens as a knob.") }
        if (-not $c.label) { $problems.Add("$where has no label, so a screen reader can't name it.") }
        $x = $c.x ?? 0; $y = $c.y ?? 0; $w = $c.width ?? 56; $h = $c.height ?? 56
        if ($w -le 0 -or $h -le 0) { $problems.Add("$where has no size.") }
        elseif ($x -lt 0 -or $y -lt 0 -or ($x + $w) -gt $pageWidth -or ($y + $h) -gt $pageHeight) { $problems.Add("$where hangs off the page, so it won't show when the layout runs.") }
        $slot = $c.hueSlot ?? 0
        if ($slot -lt -1 -or $slot -gt 6 -or ($slot -eq -1 -and -not $c.literalColor)) { $problems.Add("$where needs a hueSlot from 0 to 6, or -1 with a literalColor.") }
        foreach ($m in $c.messages) {
            $kind = $m.kind ?? 'controlChange'
            if ($kind -cnotin $messageKinds) { $problems.Add("$where sends '$kind', which isn't a message kind."); continue }
            if (($m.trigger ?? 'changes') -cnotin @('turnsOn', 'turnsOff', 'changes', 'touched', 'released')) { $problems.Add("$where has the trigger '$($m.trigger)'.") }
            if ($kind -ceq 'goToPage') {
                if ($m.targetPage -cnotin $pageIds) { $problems.Add("$where goes to a page that isn't in the layout.") }
                elseif ($m.targetPage -cin $bandIds) { $problems.Add("$where goes to a band. A band shows on every page, so it isn't somewhere to go.") }
                continue
            }
            if ($kind -ceq 'sequence') { if ($m.sequence -cnotin $sequences) { $problems.Add("$where runs a sequence that isn't in the layout.") }; continue }
            if (-not $m.device) { $problems.Add("$where has a row with no device, so the row sends nothing.") }
            elseif ($m.device -cnotin $devices) { $problems.Add("$where sends to '$($m.device)', which isn't in devices.") }
            if (($m.group ?? 0) -notin -1..15 -or ($m.channel ?? 0) -notin 0..15) { $problems.Add("$where has a group or a channel outside 0 to 15. The file counts from 0.") }
            $limit = if ($kind -cin @('registeredController', 'assignedController')) { 16383 } else { 127 }
            if (($kind -cin $numbered) -and ($m.number ?? 0) -notin 0..$limit) { $problems.Add("$where has the number $($m.number), outside 0 to $limit.") }
            if ($kind -cin @('perNoteController', 'assignablePerNoteController')) {
                $controller = $m.controller ?? 0
                if ($controller -isnot [ValueType] -or $controller -lt 0 -or $controller -gt 255) { $problems.Add("$where has the per-note controller $controller, outside 0 to 255.") }
            }
            if ($null -ne $m.attributeType -or $null -ne $m.attributeData) {
                $type = $m.attributeType ?? 0; $data = $m.attributeData ?? 0
                if ($kind -cne 'note') { $problems.Add("$where has a note attribute on a row that isn't a note, so it's never sent.") }
                elseif ($type -isnot [ValueType] -or $data -isnot [ValueType] -or $type -lt 0 -or $type -gt 255 -or $data -lt 0 -or $data -gt 65535) { $problems.Add("$where needs attributeType 0 to 255 and attributeData 0 to 65535, as decimal numbers.") }
                elseif ($type -eq 0 -and $data -ne 0) { $problems.Add("$where has attribute data with attribute type 0, so the data is never sent.") }
            }
            if ($kind -ceq 'rawUmp' -and $c.kind -cne 'beatClock') {
                $words = @($m.words)
                if ($words.Count -notin 1..4 -or @($words | Where-Object { $_ -is [string] -or $_ -lt 0 -or $_ -gt 4294967295 }).Count -gt 0) { $problems.Add("$where needs one to four words, written as decimal numbers.") }
            }
            if ($kind -ceq 'systemExclusive' -and $m.systemExclusive -notmatch '^([0-9A-Fa-f]{2})+$') { $problems.Add("$where needs its bytes as hexadecimal text with no spaces, like F07E7F0601F7.") }
        }
        if ($c.feedback.enabled -and $c.feedback.device -and $c.feedback.device -cnotin $devices) { $problems.Add("$where listens to '$($c.feedback.device)', which isn't in devices.") }
    }
}

if ($problems.Count -gt 0) { $problems; exit 1 }
'No problems found.'
```

A file that passes can still send the wrong controller. Only the manual and the customer can tell you that.

### 7. Show the customer a mockup

Show the customer what they'll get before you hand it over, and ask them to confirm the controls, the names, the colors, and what each one sends. **Draw the mockup from the file you wrote,** using its own positions and sizes, so what they approve is what they get.

**If MIDI Glass is installed and you can run commands,** it can draw the first page for you:

```powershell
midiglass --thumbnail "<layout file>" "<picture.png>" 1600
```

The last number is the width of the picture in pixels. The picture shows the page in the layout's theme, with each control as an outline in its color. It doesn't show names or values, so it's a check on positions and colors rather than a finished picture. It's drawn without opening a window or touching any device. The command returns 0 when it works, 2 when an argument is missing, 3 when the file can't be read as a layout, and 4 when the picture can't be written.

The real thing is better still: `midiglass --run "<layout file>"` opens the layout in MIDI Glass, so the customer can look at every page and try it. Opening it sends nothing until a control is touched, unless a control is set to send its value when the layout starts, or a beat clock, LFO, or Steps control is set to start running.

**If you're an online chat,** draw the mockup yourself as HTML, SVG, or a picture. This page draws every page of a layout file in Studio Dark's colors. Paste the whole layout file where it says, and change the six colors to your theme's if you know them:

```html
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<title>Layout mockup</title>
<style>
  body { margin: 24px; background: #2B2B2B; color: #E8EAED; font: 14px "Segoe UI", sans-serif; }
  .page { position: relative; overflow: hidden; margin: 8px 0 32px; background: radial-gradient(ellipse 65% 100% at 50% -10%, #1D1E21, #121316 58%, #07080A); }
  .control { position: absolute; box-sizing: border-box; border: 1px solid; border-radius: 7px; background: rgba(23, 24, 27, 0.86); }
  .round { border-radius: 50%; }
  .panel { background: rgba(23, 24, 27, 0.45); }
  .bare { border-color: transparent !important; background: none; }
  .name { position: absolute; left: -40px; right: -40px; text-align: center; color: #D7DAE0; font: 12px "Segoe UI Variable Text", "Segoe UI", sans-serif; }
  .caption { left: 10px; right: auto; text-align: left; }
</style>
<p>A mockup drawn from the layout file. It isn't a MIDI Glass screenshot.</p>
<div id="pages"></div>
<script type="application/json" id="layout">
PASTE THE WHOLE LAYOUT FILE HERE
</script>
<script>
  const layout = JSON.parse(document.getElementById('layout').textContent);
  const slots = ['#4FC3F7', '#81C784', '#FFC247', '#FF7043', '#BA68C8', '#4DD0E1'];   // Studio Dark, slots 0 to 5
  const round = ['knob', 'turntable', 'joystick', 'lamp'];
  const width = layout.pageWidth ?? 1280, height = layout.pageHeight ?? 800;
  const scale = Math.min(1, 1200 / width);
  for (const page of layout.pages) {
    const heading = document.createElement('h2');
    heading.textContent = page.name || page.id;
    const host = document.createElement('div');
    host.className = 'page';
    Object.assign(host.style, { width: width * scale + 'px', height: height * scale + 'px' });
    for (const c of page.controls ?? []) {
      const slot = c.hueSlot ?? 0;
      const box = document.createElement('div');
      box.className = 'control' + (round.includes(c.kind) ? ' round' : '') + (c.kind === 'panel' ? ' panel' : '') + (c.style === 'bare' ? ' bare' : '');
      Object.assign(box.style, {
        left: c.x * scale + 'px', top: c.y * scale + 'px',
        width: (c.width ?? 56) * scale + 'px', height: (c.height ?? 56) * scale + 'px',
        borderColor: slot === -1 ? c.literalColor : (slots[slot] ?? slots[0])
      });
      const placed = c.labelPlaced ?? (c.kind === 'panel' ? 'inside' : 'below');
      if (c.label && placed !== 'none') {
        const name = document.createElement('div');
        name.className = 'name' + (c.kind === 'panel' ? ' caption' : '');
        name.textContent = c.label;
        name.style.top = placed === 'above' ? '-18px' : placed.startsWith('inside') ? '6px' : 'calc(100% + 4px)';
        box.append(name);
      }
      host.append(box);
    }
    document.getElementById('pages').append(heading, host);
  }
</script>
</html>
```

Beside the mockup, list what each control sends in plain words, such as "Cutoff: CC 74 on channel 1" or "Filter: NRPN 7:54 on channel 1". Customers check that list more carefully than the picture.

Label the mockup as a mockup. A theme draws much more than outlines: knob arcs, fader caps, lights, and textures. If the customer wants to see the real thing, they can open the file in MIDI Glass and look before they play it.

### 8. Hand it over

Tell the customer how to get the file into MIDI Glass, using [Where layout files go, and how to bring one in](#where-layout-files-go-and-how-to-bring-one-in). Then tell them:

- Which device each name in the layout stands for, and how to point it at different hardware if a device shows as missing: open the layout in the editor, select **Layout…** > **Pages and devices…** > **Outputs**, and pick the device.
- What the layout can't do that they asked for, and what you did instead.
- Where each number came from, so they can check it against the manual.

## Where layout files go, and how to bring one in

MIDI Glass keeps its layouts in **Documents › MIDI Layouts**, one `.midilayout` file per layout. The library shows every layout file in that folder. A picture or a video a layout uses sits next to it, in the same folder.

- **The Documents folder isn't always `C:\Users\<name>\Documents`.** On many PCs it has been moved into OneDrive. In File Explorer, select **Documents** and look for **MIDI Layouts** there.
- **To add a layout to the library,** copy its file into that folder. MIDI Glass reads the folder when it starts, so if it's already open, close it and open it again to see the new layout.
- **To try a layout without adding it,** double-click the file in File Explorer, or select **Open a file…** in the MIDI Glass library. It runs in its own window from wherever the file is. The first time you double-click one, Windows asks which app to open it with: pick MIDI Glass.
- **A layout with pictures** travels as a `.zip` package. In the library, select **…** (More options) > **Import a layout package…**. Make one in MIDI Glass with **Package for another PC…** on a card's **…** menu. A package holds its files without compression, and MIDI Glass refuses a zip whose files are compressed, which is what most zip tools do. So rather than making a package yourself, give the customer the layout file and its pictures, and ask them to put them all in the layouts folder.
- Older versions of MIDI Glass named layout files `.midilayout.json`. The app still reads those, and renames them to `.midilayout` when it starts.
- Themes live in **Documents › MIDI Layouts › Themes**. See [Where theme files live]({{ site.baseurl }}/kb/midi-glass-themes/#where-theme-files-live).

**System exclusive.** A layout opened from outside the layouts folder asks before it sends system exclusive, because the wrong system exclusive can harm a device. If your layout sends system exclusive, give the customer the file outside the layouts folder, such as in Downloads, so they see that question the first time. They can copy it into the folder once they trust it.

## What MIDI Glass can't do

Tell the customer about these before they find out on their own.

- **A control can't change what another control sends.** There's no "selected articulation" that a keyboard follows, and no shift button that changes what the knobs do. A button can run a sequence that moves another control to a value, and a beat clock can take its tempo from a knob, but that's all. Use more pages, or more controls, instead.
- **A keyboard plays one attribute.** A keyboard or a pad grid can put a MIDI 2.0 note attribute, such as an articulation, on every note it plays, but it's the same attribute on every key. For two articulations, use two keyboards, or a page for each.
- **Following a clock follows only its tempo.** A layout that follows incoming MIDI clock takes its speed, but doesn't line its beat up with it, and doesn't start or stop with it.
- **Nothing shows text from a device.** A readout shows a value, and a meter and a lamp show levels and activity. There's no track name, no patch name, and no list of messages on a running page.
- **No MIDI-CI.** MIDI Glass can't turn a profile on, or ask a device what it supports.
- **No logic.** No conditions, no variables, and no scripts.
- **Mackie Control is partial.** MIDI Glass doesn't speak HUI, doesn't answer the handshake some DAWs use to find a surface, and doesn't show the DAW's meters, V-Pot rings, or display text.
- **Saved, but not working yet.** The layout's `publishesVirtualDevice` is kept in the file but doesn't do anything yet, so leave it out.

## The layout file

### A complete example

This layout has a section with two knobs, a volume fader, a pitch wheel, a hold toggle, and a pad that plays middle C. It sends to the built-in **Default App Loopback (A)**, so you can try it on any PC with Windows MIDI Services: open MIDI Monitor on **Default App Loopback (B)** and watch what it sends. For a real device, change the name in `match`.

```json
{
  "fileVersion": 1,
  "name": "Synth basics",
  "description": "Filter, volume, pitch bend, hold, and a pad for one synth.",
  "pageWidth": 1280,
  "pageHeight": 800,
  "canvasWidth": 1280,
  "canvasHeight": 800,
  "theme": "Studio Dark",
  "scaleMode": "fitToScreen",
  "devices": [
    {
      "name": "Synth",
      "match": { "transportSuppliedEndpointName": "Default App Loopback (A)" },
      "matchMode": "endpointName"
    }
  ],
  "pages": [
    {
      "id": "main",
      "name": "Main",
      "controls": [
        {
          "id": "filter-section", "kind": "panel", "label": "Filter",
          "x": 48, "y": 48, "width": 368, "height": 264,
          "hueSlot": 0, "labelPlaced": "inside", "keyboardOrder": 1
        },
        {
          "id": "cutoff", "kind": "knob", "label": "Cutoff",
          "x": 80, "y": 112, "width": 120, "height": 120,
          "hueSlot": 0, "aspectLocked": true, "keyboardOrder": 2,
          "messages": [
            { "trigger": "changes", "kind": "controlChange", "device": "Synth", "group": 0, "channel": 0, "number": 74 }
          ]
        },
        {
          "id": "resonance", "kind": "knob", "label": "Resonance",
          "x": 264, "y": 112, "width": 120, "height": 120,
          "hueSlot": 0, "aspectLocked": true, "keyboardOrder": 3,
          "messages": [
            { "trigger": "changes", "kind": "controlChange", "device": "Synth", "group": 0, "channel": 0, "number": 71 }
          ]
        },
        {
          "id": "volume", "kind": "fader", "label": "Volume",
          "x": 464, "y": 48, "width": 56, "height": 264,
          "hueSlot": 1, "defaultValue": 0.8, "keyboardOrder": 4,
          "messages": [
            { "trigger": "changes", "kind": "controlChange", "device": "Synth", "group": 0, "channel": 0, "number": 7 }
          ]
        },
        {
          "id": "bend", "kind": "wheel", "label": "Bend",
          "x": 568, "y": 48, "width": 56, "height": 264,
          "hueSlot": 4, "defaultValue": 0.5, "returnsToDefault": true, "keyboardOrder": 5,
          "messages": [
            { "trigger": "changes", "kind": "pitchBend", "device": "Synth", "group": 0, "channel": 0 }
          ]
        },
        {
          "id": "hold", "kind": "toggle", "label": "Hold",
          "x": 48, "y": 376, "width": 136, "height": 56,
          "hueSlot": 2, "keyboardOrder": 6,
          "messages": [
            { "trigger": "turnsOn", "kind": "controlChange", "device": "Synth", "group": 0, "channel": 0, "number": 64 },
            { "trigger": "turnsOff", "kind": "controlChange", "device": "Synth", "group": 0, "channel": 0, "number": 64 }
          ]
        },
        {
          "id": "middle-c", "kind": "pad", "label": "Middle C",
          "x": 232, "y": 376, "width": 96, "height": 96,
          "hueSlot": 3, "aspectLocked": true, "keyboardOrder": 7,
          "messages": [
            { "trigger": "turnsOn", "kind": "note", "device": "Synth", "group": 0, "channel": 0, "number": 60 },
            { "trigger": "turnsOff", "kind": "note", "device": "Synth", "group": 0, "channel": 0, "number": 60 }
          ]
        }
      ]
    }
  ],
  "sequences": []
}
```

The tables below list every setting. **If left out** is what MIDI Glass uses when a file doesn't have the key.

### The top level

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `fileVersion` | 1 | 1 | The file format version. Write 1. |
| `name` | text | empty | The layout's name in the library. |
| `description` | text | empty | One line about the layout, shown on its card. |
| `created`, `modified` | numbers | 0 | Kept by the app. Write 0 or leave them out. |
| `pageWidth`, `pageHeight` | 32 to 8192 | 1280, 800 | The page size in pixels. |
| `canvasWidth`, `canvasHeight` | at least the page size | the page size | The editor's working area around the page. Write the page size. |
| `theme` | a theme name | Studio Dark | The theme, by its exact name. |
| `themeColors` | a whole theme | none | A theme carried inside the layout. Leave it out unless you're designing a theme. See [A theme inside a layout]({{ site.baseurl }}/kb/midi-glass-themes/#a-theme-inside-a-layout). |
| `backgroundImage` | file name | none | A picture or a video behind the controls. Only a plain file name, of a file in the same folder as the layout. |
| `backgroundFit` | `centered`, `uniform`, `stretch`, `tiled`, `fill` | `uniform` | How the background covers the page. |
| `backgroundOpacity` | 0 to 1 | 1 | How strongly the background shows. A busy picture at full strength makes names hard to read. |
| `scaleMode` | `actualSize`, `fitToScreen`, `custom` | `actualSize` | How big the page is when the layout opens. Write `fitToScreen`. |
| `customScalePercent` | 10 to 400 | 100 | The size for `custom`. |
| `fullScreenButtonCorner` | `topLeft`, `topRight`, `bottomLeft`, `bottomRight` | `topRight` | Where the one button sits in full screen. |
| `suppressAllStartupValues` | `true`, `false` | `false` | Stops every control from sending its starting value when the layout opens. |
| `toolbarWindow`, `alwaysOnTop`, `seeThrough` | `true`, `false` | `false` | For a floating toolbar. See the [floating toolbar article]({{ site.baseurl }}/kb/midi-glass-floating-toolbars/). |
| `isFavorite` | `true`, `false` | `false` | Puts the layout in the library's **Favorites**. |
| `tempo` | object | 120 beats a minute | `{ "kind": "internal", "beatsPerMinute": 120 }`. LFO and Steps controls run at this tempo. A beat clock keeps its own. To follow the MIDI clock a device sends, write `{ "kind": "followIncomingClock", "beatsPerMinute": 120, "device": "DAW" }` with a name from `devices`. They run at `beatsPerMinute` until the clock arrives, then at the clock's tempo, and keep the last tempo if the clock stops. |
| `devices` | list | none | The device table. See [Devices](#devices). |
| `pages` | list | none | At least one page. See [Pages](#pages). |
| `sequences` | list | none | See [Sequences](#sequences). |

`_comment` is ignored. Any other key MIDI Glass doesn't know is kept and written back, but it doesn't do anything.

### Devices

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `name` | text | required | The name messages use. Short and clear, such as `Synth` or `DAW`. Each name used once. |
| `match` | object | empty | How MIDI Glass finds the real device. |
| `matchMode` | `endpointDeviceId`, `usbVendorAndProduct`, `endpointName` | `endpointDeviceId` | Which part of `match` it uses. |
| `protocol` | `midi2`, `midi1`, `mackieControl` | `midi2` | How MIDI Glass talks to the device. See [Values](#values) and [Mackie Control functions](#mackie-control-functions). |

**Matching a device by name.** You can't know a device's ID, so match by name:

```json
{ "name": "Synth", "match": { "transportSuppliedEndpointName": "Prophet Rev2" }, "matchMode": "endpointName" }
```

MIDI Glass compares the name in `match` with each device's own name and with the name Windows shows for it, which the customer may have changed in MIDI Settings. Capital letters don't matter, but everything else must be the same. If `match` has no name, it compares the device's `name` instead. When MIDI Glass saves a device the customer picked, `match` also holds the device's ID and USB details, which is how it tells two identical devices apart.

### Pages

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `id` | text | required | Unique. Page tabs go to a page by its id. |
| `name` | text | empty | The page's name in the page list. |
| `hueSlot` | -1 to 5 | 0 | The page's color slot. |
| `controls` | list | none | The controls, drawn in order: the first one is at the back. |
| `controlGroups` | list | none | Names for groups of controls that select and move together in the editor: `[ { "id": "strip-1", "name": "Strip 1" } ]`. Optional. |
| `sharedBand` | `true`, `false` | `false` | `true` makes the page a band: while the layout runs, its controls show on top of every other page, so transport buttons, panic, and page tabs only have to be built once. A band isn't in the page list, so don't give it a page tab. Leave room for its controls on every other page. |

### Controls

These keys work on every kind of control.

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `id` | text | required | Unique in the whole layout. |
| `kind` | see the table in [Lay out the page](#4-lay-out-the-page) | `knob` | What the control is. A kind MIDI Glass doesn't know opens as a knob. |
| `label` | text | empty | The control's name, on the page and for screen readers. Always set it. |
| `x`, `y` | numbers | 0 | The top left corner, in page pixels. |
| `width`, `height` | numbers above 0 | 56 | The size, in page pixels. |
| `hueSlot` | 0 to 5, 6, or -1 | 0 | The theme color. 0 to 5 are the six slots, counted from 0, so 0 is the slot the app calls 1. 6 is the theme's neutral, "no color". -1 uses `literalColor`. |
| `literalColor` | `#RRGGBB` | none | An exact color that ignores the theme. Only with `hueSlot` -1, and only for a control that must be one color, such as a red panic button. |
| `keyboardOrder` | 1 and up | 0 | The order Tab and a screen reader follow. |
| `aspectLocked` | `true`, `false` | `false` | Keeps the control's shape when it's resized in the editor. Set it on knobs, pads, XY pads, joysticks, and turntables. |
| `locked` | `true`, `false` | `false` | Stops the control from being picked or moved on the page in the editor. For a big picture behind other controls. |
| `controlGroup` | text | none | The id of an editor group in the page's `controlGroups`. |
| `defaultValue` | 0 to 1 | 0 | Where the control starts. 0.5 is the middle. |
| `defaultValueY` | 0 to 1 | 0 | Where the up and down axis of an XY pad or a joystick starts. |
| `returnsToDefault` | `true`, `false` | `false` | Springs back to `defaultValue` when let go, like a pitch wheel. |
| `sendsValueOnStart` | `true`, `false` | `false` | Sends `defaultValue` when the layout opens, to put a device in a known state. |
| `sendIntervalMilliseconds` | 0 to 10000 | 0 | The least time between two sends while the control moves. 0 is no limit. A small number, such as 10, can help a MIDI 1.0 device on a DIN cable keep up. |
| `lightsFromCenter` | `true`, `false` | `false` | Lights from the middle out, like a pan knob. Knobs and faders. |
| `drag` | `vertical`, `horizontal`, `circular` | `vertical` | Which way a finger moves to turn a knob up. |
| `velocityFromTouch` | `true`, `false` | `false` | A pad hit softly plays softly, on hardware that reports pressure. Pads. |
| `ticks` | `{ "show": true, "count": 5 }` | shown, 5 | The marks along a fader, around a knob, or across an XY pad. `count` is 2 to 64. |
| `showDetentValues` | `true`, `false` | `false` | Prints the value at each stop beside the marks. |
| `messages` | list | none | What the control sends. See [Messages](#messages). |
| `feedback` | object | none | What the control listens for. See [Listening](#listening). |
| `style`, `labelPlaced`, `labelStyle`, `showValue` | see [Names and looks](#names-and-looks) | the theme decides | |
| `pickup` | `jump`, `catch`, `relative` | `jump` | What a fader, XY pad, joystick, or ribbon does when a finger lands away from its value. `jump` moves it to the finger. `catch` leaves it until the finger reaches the value, so a fader the DAW moved doesn't jump. `relative` moves it by as far as the finger moves. |

Some kinds have one more block of settings. See [Settings for some kinds of control](#settings-for-some-kinds-of-control).

### Messages

Each row in `messages` says when the control sends, what it sends, and where.

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `trigger` | `changes`, `turnsOn`, `turnsOff`, `touched`, `released` | `changes` | When the row is sent. |
| `kind` | see below | `controlChange` | What the row sends. |
| `device` | a name from `devices` | empty | Where it goes. A row with no device sends nothing. |
| `group` | 0 to 15 | 0 | The group, counted from 0. Group 1 is 0. |
| `channel` | 0 to 15 | 0 | The channel, counted from 0. Channel 1 is 0, and channel 10 is 9. |
| `number` | 0 to 127, or 0 to 16383 | 0 | The note, controller, or program. For an RPN or NRPN, MSB × 128 + LSB. For a per-note controller, the note. |
| `controller` | 0 to 255 | 0 | For a per-note controller, which controller. |
| `attributeType`, `attributeData` | 0 to 255, and 0 to 65535 | 0 | For a `note` row sent as MIDI 2.0, the note attribute. See [Per-note controllers and note attributes](#per-note-controllers-and-note-attributes). |
| `minimum`, `maximum` | `{ "value": 0, "scaling": "fraction" }` | 0 and 1, as fractions | The two ends of what the row sends. See [Values](#values). |
| `detents` | object | none | Stops along the control's travel. See [Stops](#stops). |
| `axis` | `x`, `y` | `x` | Which value of an XY pad or a joystick the row follows. `y` is up and down. |
| `position` | 0 to 15 | every position | On a switch, the one position that sends this row, counted from 0. |
| `midi1Protocol` | `true`, `false` | `false` | Sends this row as MIDI 1.0 rather than MIDI 2.0. See [Values](#values). |
| `systemExclusive` | hexadecimal text | none | For `systemExclusive` rows. See [System exclusive](#system-exclusive). |
| `words` | one to four numbers | none | For `rawUmp` rows. See [Raw messages](#raw-messages). |
| `targetPage` | a page `id` | none | For `goToPage` rows. |
| `sequence` | a sequence `name` | none | For `sequence` rows. |
| `function` | a Mackie Control function | none | For `mackieControl` rows, instead of `number`. |

The message kinds are `note`, `controlChange`, `programChange`, `pitchBend`, `channelPressure`, `perNoteController`, `assignablePerNoteController`, `registeredController` (an RPN), `assignedController` (an NRPN), `systemExclusive`, `rawUmp`, `sequence`, `goToPage`, and `mackieControl`.

**When each trigger sends:**

- A `changes` row is sent whenever the control moves, at the control's position between `minimum` and `maximum`. On a button, a toggle, or a pad, it sends the maximum when the control turns on and the minimum when it turns off.
- A `turnsOn` row is sent when a button, toggle, or pad turns on, and sends its `maximum`. A `turnsOff` row is sent when it turns off, and sends its `minimum`.
- A `touched` row is sent when a finger first touches the control, and sends its `maximum`. A `released` row is sent when the finger lets go, and sends its `minimum`.
- Rows on the same trigger go out in the order they're listed.

**What each kind of control usually sends.** These are the rows MIDI Glass gives a new control. Copy their shape.

| Control | Rows |
| --- | --- |
| Knob, fader, ribbon, LFO | One `changes` row, usually a `controlChange`. |
| Button, toggle, pad | A `turnsOn` row and a `turnsOff` row, with the same message. For a note, that's a note on and a note off. |
| Wheel, turntable | One `changes` row, usually `pitchBend`, with `defaultValue` 0.5 and `returnsToDefault` true. For a modulation wheel, use CC 1 with `defaultValue` 0 and `returnsToDefault` false. |
| XY pad, joystick | Two `changes` rows, one with `axis` `x` and one with `axis` `y`. |
| Switch | One `changes` row for each position, each with its `position` and a `maximum`. |
| Page tab | One `turnsOn` row of kind `goToPage`, with `targetPage` and no device. |
| Mono keyboard, note pads, hex pads, steps | One `changes` row of kind `note`, with no `number`. The key or the step decides the note. |
| Beat clock | One `changes` row of kind `rawUmp`, with no `words`. It only says where the clock goes. |
| Meter, lamp, readout, stopwatch, text, image, group, line | No rows. Meters, lamps, and readouts listen instead. |

### Values

The two ends of a row, `minimum` and `maximum`, are each an object with a `value` and a `scaling`. A control at rest sends the minimum, a control at full travel sends the maximum, and anything between is in proportion. Leave both out for the full range.

- **`fraction`** is a share of the full range, from 0 to 1. It works on MIDI 1.0 and MIDI 2.0 devices alike, so use it unless you have a reason not to. To send exactly 100 out of 127, write 100 ÷ 127, about `0.7874`. MIDI Glass rounds to the nearest step.
- **`absolute`** is the exact number that goes on the wire. Its range depends on the message: 0 to 127 for a MIDI 1.0 note velocity, controller, or channel pressure; 0 to 16383 for MIDI 1.0 pitch bend; 0 to 65535 for a MIDI 2.0 note velocity; and 0 to 4294967295 for every other MIDI 2.0 value.
- **A row goes out as MIDI 1.0** when its device's `protocol` is `midi1`, or when the row has `"midi1Protocol": true`. That only applies to notes, controllers, program changes, pitch bend, and channel pressure. RPNs, NRPNs, and per-note controllers always go out as MIDI 2.0, and Windows converts them for a MIDI 1.0 device. Every other row goes out as MIDI 2.0, and Windows converts it for a MIDI 1.0 device too.
- **When a value is a code,** such as a pad color where 5 is red and 21 is green, set the device's `protocol` to `midi1` and write the code as an `absolute` value, so the device gets exactly that number.
- **A minimum above the maximum** turns the control upside down.
- **A note** is on when its value is at least half way, and off below that. A `turnsOn` row plays the note at the `maximum` velocity, and a `turnsOff` row sends a note off with the `minimum` as its release velocity.
- **A program change** sends its `number`. Its value doesn't matter.

> **For agents:** Don't write `absolute` values for a MIDI 2.0 device unless you mean MIDI 2.0 numbers. `"value": 127` with `absolute` on a MIDI 2.0 controller is 127 out of 4294967295, which is next to nothing.

### Stops

`detents` gives a control stops, like a selector:

- `{ "mode": "evenSteps", "scaling": "fraction", "step": 0.25 }` stops at 0, 25, 50, 75, and 100 percent.
- `{ "mode": "explicitValues", "scaling": "absolute", "stops": [ 10, 17, 38, 39, 40, 57 ] }` stops at exactly those values. Each stop gets an equal share of the travel, wherever its value falls.
- `"mode": "continuous"` is no stops, the same as leaving `detents` out.

`step` and `stops` use the same units as `scaling`.

### Per-note controllers and note attributes

Both are MIDI 2.0 only. A row that goes out as MIDI 1.0 can't carry them.

**A per-note controller** changes one note that's playing, not the whole channel. Its row has two numbers: `number` is the note, and `controller` is which controller, from 0 to 255. Use `perNoteController` for the controllers the MIDI 2.0 specification names, such as 7 for volume and 10 for pan, and `assignablePerNoteController` for ones the device defines itself. Take the numbers from the device's manual.

```json
{ "trigger": "changes", "kind": "perNoteController", "device": "Synth", "group": 0, "channel": 0, "number": 60, "controller": 7 }
```

**A note attribute** goes out with a note on or a note off, and tells the device something more about the note, such as which articulation to play. Add `attributeType` and `attributeData` to a `note` row, as decimal numbers. Type 0 is no attribute, 1 is manufacturer specific, 2 is profile specific, and 3 is pitch 7.9. Take the type and the data from the device's manual or the profile's specification. The editor shows both in hexadecimal.

```json
{ "trigger": "turnsOn", "kind": "note", "device": "Synth", "group": 0, "channel": 0, "number": 60, "attributeType": 2, "attributeData": 1 }
```

- Each row carries only its own attribute. If the device wants one on the note off too, put it on the `turnsOff` row as well.
- A keyboard or a pad grid puts its row's attribute on every note it plays, on and off.

### Listening

A control with a `feedback` block moves or lights up when a device sends something, not only when it's touched. That's how a fader follows a DAW, and how a lamp shows activity.

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `enabled` | `true`, `false` | `false` | Turns listening on. |
| `mode` | `message`, `anyActivity`, `notes`, `controlChanges`, `transport`, `tempo` | `message` | `message` follows one controller, note, or pitch bend, and is the only mode that carries a value. `anyActivity` lights on anything at all. `notes` lights while notes arrive. `controlChanges` blinks on any controller. `transport` lights from start to stop. `tempo` flashes on the beat. |
| `kind` | a message kind | `controlChange` | For `message` mode. |
| `device` | a name from `devices` | empty | Where to listen. |
| `group` | -1 to 15 | 0 | Counted from 0. -1 is any group. |
| `channel` | 0 to 15 | 0 | Counted from 0. |
| `number` | 0 to 127 | 0 | The controller or note, for `message` mode. |
| `matchesChannel` | `true`, `false` | `false` | In `anyActivity` mode, only counts `channel`. |
| `tempoControl` | a control `id` | empty | In `tempo` mode, a beat clock on this layout to follow. Empty follows the clock arriving from `device`. |
| `holdMilliseconds` | 0 to 10000 | 120 | How long a lamp stays lit after something arrives. |

```json
"feedback": { "enabled": true, "mode": "message", "kind": "controlChange", "device": "DAW", "group": 0, "channel": 0, "number": 7 }
```

### Settings for some kinds of control

**Mono keyboard,** in `keyboard`:

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `keyCount` | 5 to 128 | 25 | How many keys, black and white together. |
| `lowestNote` | 0 to 127 | 48 | The note of the leftmost key. 48 is C2. |
| `whiteKeyColor`, `blackKeyColor`, `pressedKeyColor` | `#RRGGBB` | the theme's | Leave them out to follow the theme. |
| `velocityFromKeyPosition` | `true`, `false` | `false` | A key pressed nearer its bottom end plays louder. |
| `showNoteNames` | `true`, `false` | `false` | Prints each key's note name and number on it, the way the MIDI Keyboard app does. They only show when they fit: white keys at least 22 pixels wide, on a keyboard at least 70 pixels tall. |

**Note pads and hex pads,** in `pads`:

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `padCount` | 1 to 128 | 24 | How many pads. They flow into rows to fill the control's width. |
| `padSize` | 12 to 240 | 48 | One pad's width, in page pixels. |
| `startNote` | 0 to 127 | 48 | The note of the bottom left pad. |
| `rightInterval` | 1 to 24 | 1 | Semitones from one pad to the next on its right. |
| `rowInterval` | 0 to 24 | 5 | Semitones from one pad to the one above it. On square pads, 0 carries each row on from the end of the row below. |
| `key` | `none`, `c`, `cSharp`, `d`, `dSharp`, `e`, `f`, `fSharp`, `g`, `gSharp`, `a`, `aSharp`, `b` | `c` | The key the pads are colored for. |
| `scale` | `major`, `minor`, `harmonicMinor`, `melodicMinor`, `dorian`, `phrygian`, `lydian`, `mixolydian`, `locrian`, `majorPentatonic`, `minorPentatonic`, `blues`, `wholeTone` | `major` | |
| `noteNames` | `hidden`, `center`, `top`, `bottom`, `topLeft`, `topRight`, `bottomLeft`, `bottomRight` | `center` | Where each pad prints its note. |
| `noteNameSize` | 0 to 64 | 0 | 0 sizes the name to the pad. |
| `rootColor`, `inKeyColor`, `outOfKeyColor`, `pressedColor` | `#RRGGBB` | the theme's | Leave them out to follow the theme. |
| `glide` | `off`, `portamento`, `perNoteBend` | `off` | What a finger sliding onto the next pad does. `perNoteBend` bends the note with MIDI 2.0 per-note pitch bend. |
| `bendRange` | 1 to 96 | 48 | Semitones either way, for `perNoteBend`. |

**Switch,** in `switch`: `{ "positions": [ "Saw", "Square", "Sine" ] }`, two to sixteen names, first to last. Give the switch one row per position:

```json
"switch": { "positions": [ "Saw", "Square", "Sine" ] },
"messages": [
  { "trigger": "changes", "kind": "controlChange", "device": "Synth", "number": 70, "position": 0, "maximum": { "value": 0, "scaling": "fraction" } },
  { "trigger": "changes", "kind": "controlChange", "device": "Synth", "number": 70, "position": 1, "maximum": { "value": 0.5, "scaling": "fraction" } },
  { "trigger": "changes", "kind": "controlChange", "device": "Synth", "number": 70, "position": 2, "maximum": { "value": 1, "scaling": "fraction" } }
]
```

**Steps,** in `sequencer`:

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `pattern` | 1 to 64 steps | none | Each step is `{ "on": true, "note": 60, "velocity": 0.8 }`. A step that's off is a rest. `velocity` is 0 to 1. |
| `stepsPerBeat` | 0.25 to 8 | 4 | 4 is sixteenth notes. |
| `gate` | 0.05 to 1 | 0.5 | How much of its step each note sounds. |
| `swing` | 0.5 to 0.75 | 0.5 | How late every second step lands. 0.5 is straight. |
| `direction` | `forward`, `backward`, `pingPong`, `random` | `forward` | |
| `latching` | `true`, `false` | `true` | Press to start and press again to stop, rather than running only while held. |
| `startsRunning` | `true`, `false` | `false` | Runs as soon as the layout opens. |

**LFO,** in `lfo`:

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `wave` | `sine`, `triangle`, `square`, `rampUp`, `rampDown`, `whiteNoise`, `pinkNoise`, `brownNoise`, `blueNoise` | `sine` | |
| `beatsPerCycle` | 0.0625 to 64 | 4 | How long one sweep takes, in beats of the layout's tempo. 4 is one bar. |
| `lowest`, `highest` | 0 to 1 | 0 and 1 | The two ends of the sweep, along the control's range. |
| `updateMilliseconds` | 5 to 1000 | 25 | How often a value is sent. |
| `latching` | `true`, `false` | `true` | Press to start and press again to stop. |
| `startsRunning` | `true`, `false` | `false` | Runs as soon as the layout opens. |
| `returnsToRestWhenStopped` | `true`, `false` | `true` | Goes back to `defaultValue` when stopped. |

**Beat clock,** in `clock`:

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `beatsPerMinute` | 20 to 300 | 120 | The tempo. |
| `tempoControl` | a control `id` | empty | A knob or a fader whose value sets the tempo instead. |
| `lowestBeatsPerMinute`, `highestBeatsPerMinute` | 20 to 300 | 40 and 240 | What that control's two ends mean. |
| `startsRunning` | `true`, `false` | `false` | Runs as soon as the layout opens. |
| `sendsTransport` | `true`, `false` | `true` | Sends start and stop around the clock. |

**Turntable,** in `turntable`: `degreesForFullRange`, 15 to 1440 with 180 if left out, is how far the platter turns for its full range, and `showsGrip`, `true` if left out, draws ridges around its edge.

**Line,** in `line`: `thickness`, 1 to 64 with 1 if left out; `color` as `#RRGGBB`, the theme's if left out; and `ends`, which is `useTheme`, `square`, or `faded`. A line runs the long way across its rectangle. Give it `"labelPlaced": "none"`.

**Image,** in `picture`:

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `file` | file name | none | A `.png`, `.jpg`, `.jpeg`, or `.svg` picture, or a video such as `.mp4`, in the same folder as the layout. Only a plain file name. A path is ignored. |
| `fit` | `centered`, `uniform`, `stretch`, `tiled`, `fill` | `uniform` | How the picture fills the control. |
| `opacity` | 0 to 1 | 1 | |
| `zoom` | 1 to 8 | 1 | Draws it larger, cut off at the control's edges. |
| `centerX`, `centerY` | 0 to 1 | 0.5 | Which point of the picture sits in the middle of the control. |
| `tint`, `tintStrength` | `#RRGGBB`, 0 to 1 | none, 0 | A wash of color over the picture. Black dims it. |
| `loops`, `autoPlays` | `true`, `false` | `true` | For a video. |
| `startSeconds`, `endSeconds` | numbers | 0 | The part of a video that plays. An end of 0 is the end of the file. |
| `clickToPlay`, `showsScrubber` | `true`, `false` | `false` | A click starts and stops a video, and a bar along the bottom moves through it. |

A Group can carry a `picture` too, which fills its frame.

### Names and looks

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `labelPlaced` | `below`, `above`, `inside`, `insideTop`, `insideCenter`, `insideBottom`, `insideTopLeft`, `verticalLeft`, `verticalRight`, `none`, `custom` | the theme decides | Where the name goes. Give a Group `inside`, Text `insideCenter`, and a Line `none`. `custom` needs the box in `labelStyle`. |
| `style` | `plate`, `outline`, `solid`, `bare` | the theme decides | How one control differs from the theme. Give Text `bare`. Otherwise leave it out, unless one control has to stand apart, like a panic button. |
| `showValue` | `always`, `whileTouched`, `never` | the theme decides | When the control's value is printed. |
| `labelStyle` | object | none | `fontFamily`, a font name only; `fontSize`, 0 for the theme's 12; `fontWeight`, such as 600; `italic`; `underline`; `color` as `#RRGGBB`; `wrap`, `true` if left out; `widthPercent`, 10 to 400; and `boxX`, `boxY`, `boxWidth`, `boxHeight` for a `custom` placement, measured from the control's top left corner. |

### Sequences

A sequence is a list of steps that a button runs, in `sequences` at the top of the file. A control runs it with a row of kind `sequence`.

| Key | Values | If left out | What it does |
| --- | --- | --- | --- |
| `name` | text | required | Unique. Rows run a sequence by its name. |
| `mode` | `once`, `whileHeld`, `toggle` | `once` | Runs once per press, repeats while held, or starts on one press and stops on the next. |
| `steps` | list, up to 512 | none | The steps, in order. |

| Step `kind` | Other keys | What it does |
| --- | --- | --- |
| `sendMessage` | `message`, a message row | Sends the row's `maximum`. A note step plays the note for `durationMilliseconds`, 200 if left out, then ends it. |
| `sendSystemExclusive` | `message`, a `systemExclusive` row | |
| `wait` | `waitMilliseconds` | Pauses. |
| `setControlValue` | `targetControl`, a control `id`, and `targetValue`, 0 to 1 | Moves another control, which then sends its own rows. |
| `goToPage` | `message`, a `goToPage` row | |
| `repeatStart`, `repeatEnd` | `repeatCount` on `repeatStart` | Repeats the steps between them. |

This button selects bank 0, then program 12 as a manual counts it, then sets the volume:

```json
"sequences": [
  {
    "name": "Load patch 12",
    "mode": "once",
    "steps": [
      { "kind": "sendMessage", "message": { "kind": "controlChange", "device": "Synth", "number": 0, "maximum": { "value": 0, "scaling": "fraction" } } },
      { "kind": "sendMessage", "message": { "kind": "programChange", "device": "Synth", "number": 11 } },
      { "kind": "wait", "waitMilliseconds": 50 },
      { "kind": "sendMessage", "message": { "kind": "controlChange", "device": "Synth", "number": 7, "maximum": { "value": 0.7874, "scaling": "fraction" } } }
    ]
  }
]
```

```json
{
  "id": "patch-12", "kind": "button", "label": "Patch 12",
  "x": 48, "y": 520, "width": 136, "height": 56, "keyboardOrder": 8,
  "messages": [ { "trigger": "turnsOn", "kind": "sequence", "sequence": "Load patch 12" } ]
}
```

### Pages and page tabs

Every page needs a tab for every page, so the customer can get back. A tab's id has to be unique, so the copies on each page need ids of their own. Or put the tabs on a [band](#pages) once, and every page shows them.

```json
"pages": [
  {
    "id": "play", "name": "Play",
    "controls": [
      { "id": "play-tab-play", "kind": "pageTab", "label": "Play", "x": 48, "y": 16, "width": 120, "height": 36, "keyboardOrder": 1,
        "messages": [ { "trigger": "turnsOn", "kind": "goToPage", "targetPage": "play" } ] },
      { "id": "play-tab-mix", "kind": "pageTab", "label": "Mix", "x": 176, "y": 16, "width": 120, "height": 36, "keyboardOrder": 2,
        "messages": [ { "trigger": "turnsOn", "kind": "goToPage", "targetPage": "mix" } ] }
    ]
  },
  {
    "id": "mix", "name": "Mix",
    "controls": [
      { "id": "mix-tab-play", "kind": "pageTab", "label": "Play", "x": 48, "y": 16, "width": 120, "height": 36, "keyboardOrder": 1,
        "messages": [ { "trigger": "turnsOn", "kind": "goToPage", "targetPage": "play" } ] },
      { "id": "mix-tab-mix", "kind": "pageTab", "label": "Mix", "x": 176, "y": 16, "width": 120, "height": 36, "keyboardOrder": 2,
        "messages": [ { "trigger": "turnsOn", "kind": "goToPage", "targetPage": "mix" } ] }
    ]
  }
]
```

### System exclusive

Write the bytes as hexadecimal text with no spaces, in `systemExclusive`, with or without the `F0` at the start and the `F7` at the end: `"systemExclusive": "F07E7F0601F7"`. Every byte between them must be 7F or below. A message can be up to 512 kilobytes.

### Raw messages

A `rawUmp` row sends one to four 32-bit words exactly as written. It's how a button sends a message MIDI Glass has no row for.

- **The words are decimal numbers.** JSON has no hexadecimal, so `0x40903C02` must be written as `1083194370`. In PowerShell, `[Convert]::ToUInt32('40903C02', 16)` does the conversion.
- **The words carry their own group and channel.** The row's `group` and `channel` aren't used, but its `device` is.

This pad sends a MIDI 2.0 note on for middle C on group 1 and channel 1, at velocity `C000`, with attribute type `02` and attribute data `0001`, and ends it with a note off. It's the same as a `note` row with an [attribute](#per-note-controllers-and-note-attributes), written out by hand to show how the words work.

```json
{
  "id": "articulation-pad", "kind": "pad", "label": "Middle C, attribute 1",
  "x": 48, "y": 600, "width": 96, "height": 96, "aspectLocked": true, "keyboardOrder": 9,
  "messages": [
    { "trigger": "turnsOn", "kind": "rawUmp", "device": "Synth", "words": [ 1083194370, 3221225473 ] },
    { "trigger": "turnsOff", "kind": "rawUmp", "device": "Synth", "words": [ 1082145792, 0 ] }
  ]
}
```

Those words are `40903C02 C0000001` and `40803C00 00000000`.

### Mackie Control functions

To control a DAW set up for a Mackie Control surface, give its device `"protocol": "mackieControl"`, and give each control one `changes` row of kind `mackieControl` with a `function` instead of a `number`: `{ "trigger": "changes", "kind": "mackieControl", "device": "DAW", "function": "play" }`. The function sends the press and the release, or the position and the touch, by itself. [How to control your DAW with Mackie Control in MIDI Glass]({{ site.baseurl }}/kb/midi-glass-mackie-control/) explains the DAW side.

| Controls | Functions |
| --- | --- |
| Button, toggle, or pad | `rewind`, `fastForward`, `stop`, `play`, `record`, `cycle`, `click`, `marker`, `nudge`, `drop`, `replace`, `scrub`, `zoom`; `rec1` to `rec8`, `solo1` to `solo8`, `mute1` to `mute8`, `select1` to `select8`, `vpotPress1` to `vpotPress8`; `bankLeft`, `bankRight`, `channelLeft`, `channelRight`, `up`, `down`, `left`, `right`; `f1` to `f8`; `assignTrack`, `assignSend`, `assignPan`, `assignPlugIn`, `assignEq`, `assignInstrument`, `flip`, `globalView`, `nameValue`, `smpteBeats`, `midiTracks`, `inputs`, `audioTracks`, `audioInstruments`, `aux`, `buses`, `outputs`, `user`; `shift`, `option`, `control`, `alt`; `readOff`, `write`, `trim`, `touch`, `latch`, `group`; `save`, `undo`, `cancel`, `enter`, `globalSolo`, `userSwitch1`, `userSwitch2` |
| Fader | `fader1` to `fader8`, `masterFader` |
| Knob, wheel, or turntable | `vpot1` to `vpot8`, `jog` |

### Limits

MIDI Glass reads a layout up to 16 megabytes, with up to 64 pages, 1,024 controls on a page, 32 rows on a control, 32 devices, and 256 sequences of up to 512 steps. Text is cut off at 1,024 characters. Anything past a limit is dropped.

## Mistakes that are easy to miss

> **For agents:** Each of these loads without an error and gives the customer the wrong layout.
>
> - A group or a channel is counted from 1. Channel 10 is `9` in the file.
> - A program is counted from 1. Program 1 is `0` in the file.
> - An NRPN or RPN is written as the LSB alone, or as two rows of CC 99 and CC 98. Write one `assignedController` or `registeredController` row with MSB × 128 + LSB.
> - A theme name is misspelled or wrongly capitalized, such as `Studio dark`, so the customer sees Studio Dark anyway, or `high contrast` and they see Studio Dark instead of High contrast.
> - A device in `match` is a name that's only close to the real one, so the device shows as missing.
> - A message names a device that isn't in `devices`, or spells it differently, so it sends nothing.
> - A button, toggle, or pad has a `turnsOn` note row but no `turnsOff` row, so the note never ends.
> - The `turnsOn` and `turnsOff` rows of a pair have different notes, channels, or devices, so the note off goes somewhere else and the note keeps playing.
> - A value for a MIDI 2.0 device is written as an `absolute` 0 to 127, so it's next to nothing.
> - A hexadecimal number is written in JSON, such as `0x40903C02` or `"40903C02"` in `words`. Only decimal numbers work.
> - A per-note controller has its two numbers swapped. `number` is the note, and `controller` is the controller.
> - A note attribute is only on the `turnsOn` row of a pair, when the device wants it on the note off too.
> - A control on a band sits where it covers a control on another page.
> - A control hangs off the edge of the page, so it doesn't show.
> - A Group is listed after the controls it frames, so it's drawn over them.
> - Two controls share an id, often page tabs copied from one page to the next.
> - A control has no `label`, so a screen reader has nothing to say.
> - A control's `kind` is misspelled, such as `xypad` or `slider`, so it opens as a knob.
> - A picture is named by a path. Only a plain file name, of a file next to the layout, is used.
> - A control is set to send its value when the layout opens, and it's a volume at 0, which mutes the device the moment the layout opens.
> - The layout depends on something in [What MIDI Glass can't do](#what-midi-glass-cant-do).
