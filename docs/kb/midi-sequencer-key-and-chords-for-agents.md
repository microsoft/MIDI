---
layout: kb
title: Windows MIDI Sequencer song plans, a guide for AI agents
audience: everyone
description: How an AI agent or an online AI chat writes the key, the chords, and the sections of a whole song for the Key and chords row in Windows MIDI Sequencer. What to ask, the song plan format, the checks the app makes, and how the person puts the plan into their sequence.
categories:
  - Developer Guidance
---

Windows MIDI Sequencer is the Windows MIDI Services app that puts MIDI on a timeline, so you can build a song and play it to your devices. A sequence can have a **Key and chords** row, under **Tempo and meter**, that holds the song's key and its chords. An AI assistant can write the key, the chords, and the sections for a whole song. The person pastes the assistant's answer into Windows MIDI Sequencer, and the app puts the keys and chords on the row and a tag for each section, like Verse and Chorus, on **Tempo and meter**. That answer is called a **song plan**.

This article is written for AI agents and online AI chats that write song plans for people. It's also for anyone who wants to write or check a song plan by hand. If you're asking an AI to plan the chords for your song, give it the link to this article and ask it to read the whole page before it starts.

> **Windows MIDI Sequencer is a preview app.** The song plan described here is version 1. Later versions can add settings, and Windows MIDI Sequencer keeps reading version 1 plans when they do.

> **For AI agents:** Read this article from start to finish before you write anything. Follow [Writing a song plan for someone](#writing-a-song-plan-for-someone) as your process, and use [The song plan](#the-song-plan) as your reference. You can't see Windows MIDI Sequencer yourself. The app checks every answer and refuses one that has any mistake in it, so a plan that's nearly right still does nothing until it's fixed.

The rules that matter most:

1. **Ask before you write.** Ask one question at a time, and offer a sensible answer for each so the person can just say yes.
2. **Write only the key, the chords, and the sections.** No notes, drums, tempo changes, or lyrics. Windows MIDI Sequencer has nowhere to put them.
3. **Use the sequence's meter.** If the prompt says the sequence is in 3/4, the plan is in 3/4.
4. **Make each section's chords fill it exactly.** The beats of a section's chords add up to its bars times the beats in a bar. This is the mistake the app refuses most often.
5. **Use only the chord types and scales in this article,** spelled exactly the way it shows them.
6. **Put the plan in one code block marked `midi-sequencer-song`,** and write strict JSON. No comments, and no comma after the last item.
7. **Say what Windows MIDI Sequencer can't do** before the person finds out. See [What Windows MIDI Sequencer can't do](#what-sequencer-cant-do).

On this page:

- [What a song plan is](#what-a-song-plan-is)
- [Writing a song plan for someone](#writing-a-song-plan-for-someone)
- [What the app does with the plan](#what-the-app-does-with-the-plan)
- [What Windows MIDI Sequencer can't do](#what-sequencer-cant-do)
- [The song plan](#the-song-plan)
- [The checks the app makes](#the-checks-the-app-makes)
- [Mistakes that are easy to miss](#mistakes-that-are-easy-to-miss)

## What a song plan is

- A song plan has a **key** at the start of the song: a root note, like A, and a scale, like natural minor.
- It has **sections**, in the order they play, such as Intro, Verse, Chorus, and Outro. Each section has a name and a length in bars.
- Each section has **chords**, in the order they play. Each chord has a name, like Dm7 or C/E, and a length in beats. A chord lasts until the next one starts. "N.C." means no chord, for a stop or a break.
- A section can **change key** at its start. The new key lasts until the next section that changes key.
- That's all. A song plan has no notes, so it doesn't play anything by itself. The person turns the chords into music in Windows MIDI Sequencer afterward, as step 6 of [Writing a song plan for someone](#writing-a-song-plan-for-someone) explains.

## Writing a song plan for someone

The person starts in Windows MIDI Sequencer. They right-click the **Key and chords** row, or its header, and select **Ask an AI assistant…**. A dialog opens with a starting prompt and a **Copy** button. They paste that prompt into a chat with you. Windows MIDI Sequencer doesn't send anything anywhere by itself.

The prompt tells you the sequence's meter and tempo, any meter changes, such as "From bar 33 it's in 3/4", its current key if it has one, and the tags it already has, such as an existing Verse at bar 9. Use what it says, and don't ask for it again.

### 1. Ask first

Ask these one at a time, in plain words. Offer a sensible answer with each one, so the person can just say yes. Skip any question the person has already answered.

| Ask | A sensible answer to offer | Why it matters |
| --- | --- | --- |
| What style or genre is the song, and what mood? | Whatever they've already told you. Otherwise ask, with two or three examples, like "upbeat pop", "slow ballad", or "12-bar blues". | It decides the structure, the key, and the kind of chords. |
| What's the song's structure, and how many bars is each section? | One that suits the style. For pop: Intro 4, Verse 8, Pre-chorus 4, Chorus 8, Verse 8, Chorus 8, Bridge 8, Chorus 8, Outro 4. For a blues: 12-bar choruses. | Each section becomes a tag on **Tempo and meter**, and its length decides how many chords it needs. |
| What key? Or should you choose? Any key changes? | The current key from the prompt, if there is one. Otherwise a key that suits the style, such as A minor for a sad ballad. Offer a common key change, like the last chorus going up a whole step. | Everything else on the row follows the key. |
| How rich should the chords be? | **Simple** (three-note chords, called triads) for pop, rock, and folk. **Sevenths** for soul, R&B, and funk. **Jazz** (sevenths plus extended chords, like 9, 11, and 13) for jazz. | It decides which chord types you use. |
| How often should the chords change? | One chord a bar for most songs. Two a bar for faster harmony, or one every two bars for something slow and open. | It decides each chord's beats. |

> **For agents:** Don't ask about meter or tempo when the prompt gives them. The plan has to use the sequence's meter, and a song plan can't change the tempo. If the person wants a different meter, they change it on **Tempo and meter** before they paste your plan.

### 2. Plan the chords

- **Work out each section's beats first.** A section's chords fill it exactly: their beats add up to the section's bars times the beats in one bar. Count the beats again before you write.
- **A beat is the note on the bottom of the meter.** So a bar of 4/4 has 4 beats, 3/4 has 3, 6/8 has 6 eighth-note beats, and 2/2 has 2 half-note beats. The number of beats in a bar is always the top number.
- **Use only the chord types in [Chord types](#chord-types).** When a style calls for a chord that isn't there, use the nearest one, and tell the person what you changed. For example, write C7 for C7♯9, and C for a C5 power chord.
- **Keep the chords in the section's key,** apart from the borrowed and passing chords the style uses. The row marks a chord with notes from outside the key with a star. That isn't an error, just something the person can see.
- **Keep the song inside one meter.** If the prompt lists a meter change, the song can't run across it. Plan a song that ends before the change, or ask the person where the song should go. A song placed across a meter change is refused.
- **Give each section a short, plain name.** Repeat a name for a section that comes back, like Chorus, or number them, like Verse 1 and Verse 2. Each name becomes a tag.

| Bars | Meter | Beats the chords add up to |
| --- | --- | --- |
| 8 | 4/4 | 32 |
| 4 | 3/4 | 12 |
| 8 | 6/8 | 48 |
| 12 | 4/4 | 48 |
| 16 | 2/2 | 32 |

### 3. Write the answer

- **Put the plain-words plan from step 5 first.** Windows MIDI Sequencer ignores everything outside the code block, so the person can read your summary and still paste your whole answer.
- **Then write the plan in one code block** whose first line is <code>```midi-sequencer-song</code>. Use this fence, not `json`.
- **Write strict JSON.** No comments, no comma after the last item in a list or an object, and straight double quotes.
- **Spell names exactly as this article shows them,** capital letters included: `naturalMinor`, `maj7`, `N.C.`.
- **Put only one song in one answer.** If the person wants changes, write the whole plan again.

### 4. Check it

Go through [Mistakes that are easy to miss](#mistakes-that-are-easy-to-miss) for every plan. If you can run PowerShell 7, save this as `Test-MidiSequencerSong.ps1`, save your whole answer to a text file, and run `pwsh -File Test-MidiSequencerSong.ps1 -Path "<answer file>"`. It finds the plan in the answer the same way Windows MIDI Sequencer does, and makes the same checks. If the plan leaves out `meter`, add `-BeatsPerBar` with the top number of the sequence's meter, such as `-BeatsPerBar 3` for 3/4. Without either one, it uses 4.

```powershell
param(
    [Parameter(Mandatory)][string]$Path,
    [int]$BeatsPerBar = 0
)

$text = [IO.File]::ReadAllText($Path)
$problems = [Collections.Generic.List[string]]::new()

# The JSON: the block fenced as midi-sequencer-song, else one fenced as json, else from the first { to the last }.
$songFence = '(?ms)^[ \t]*```[ \t]*midi-sequencer-song[ \t]*\r?\n(.*?)^[ \t]*```'
$jsonFence = '(?ms)^[ \t]*```[ \t]*json[ \t]*\r?\n(.*?)^[ \t]*```'
$blocks = [regex]::Matches($text, $songFence)
$warnings = [Collections.Generic.List[string]]::new()
if ($blocks.Count -gt 1) { $warnings.Add("Warning: the answer has $($blocks.Count) midi-sequencer-song blocks. The app reads only the first. Put the whole song in one.") }
$fence = if ($blocks.Count -gt 0) { $blocks[0] } else { [regex]::Match($text, $jsonFence) }
if ($fence.Success) { $json = $fence.Groups[1].Value }
else {
    $first = $text.IndexOf('{')
    $last = $text.LastIndexOf('}')
    if ($first -lt 0 -or $last -lt $first) { 'There is no JSON object in the answer.'; '1 problem'; exit 1 }
    $json = $text.Substring($first, $last - $first + 1)
}

try {
    $null = [Text.Json.JsonDocument]::Parse($json)   # throws on a comment or a comma after the last item
    $song = $json | ConvertFrom-Json
}
catch {
    $e = $_.Exception
    while ($e.InnerException) { $e = $e.InnerException }
    "The answer doesn't read as JSON: $($e.Message)"; '1 problem'; exit 1
}
if ($song -isnot [pscustomobject]) { 'The JSON has to be one object, in { and }.'; '1 problem'; exit 1 }

$sharp = "`u{266F}"
$flat = "`u{266D}"
$spaces = " `t`r`n`u{00A0}".ToCharArray()
$notePattern = "^[A-G][#b$sharp$flat]?$"
$scales = 'major', 'naturalMinor', 'dorian', 'phrygian', 'lydian', 'mixolydian', 'harmonicMinor', 'majorPentatonic', 'minorPentatonic', 'blues', 'chromatic'
$types = @(
    '', 'maj', 'M', 'm', 'min', '-', '7', 'maj7', 'M7', 'm7', 'min7', '-7', 'dim', "`u{00B0}", 'sus4', 'sus', 'sus2',
    '6', 'm6', 'min6', '-6', '9', 'maj9', 'M9', 'm9', 'min9', '-9', 'add9', 'aug', '+', 'dim7', "`u{00B0}7",
    "m7${flat}5", 'm7b5', 'min7b5', '-7b5', "`u{00F8}", "`u{00F8}7", '7sus4', '7sus', '11', '13')

function Get-Whole($value) {
    if ($value -is [long] -or $value -is [int]) { return [long]$value }
    if (($value -is [double] -or $value -is [decimal]) -and $value -eq [math]::Truncate($value) -and [math]::Abs($value) -lt 1e15) { return [long]$value }
    return $null
}

function Get-Pitch([string]$note) {
    $pitch = @{ C = 0; D = 2; E = 4; F = 5; G = 7; A = 9; B = 11 }[[string]$note[0]]
    if ($note.Length -gt 1) { $pitch += if ($note[1] -eq '#' -or [string]$note[1] -eq $sharp) { 1 } else { 11 } }
    return $pitch % 12
}

# A chord's text: 'none' for no chord, 'chord' for a chord, or $null when the app can't read it.
function Read-Chord([string]$chord) {
    $chord = $chord.Trim($spaces)
    if ($chord -ceq 'N.C.' -or $chord -ceq 'N.C' -or $chord -ceq 'NC') { return 'none' }
    $parts = [regex]::Match($chord, "(?s)^[A-G][#b$sharp$flat]?(.*)$")
    if (-not $parts.Success) { return $null }
    $suffix = $parts.Groups[1].Value
    $slash = $suffix.IndexOf('/')
    if ($slash -ge 0) {
        if ($suffix.Substring($slash + 1).Trim($spaces) -cnotmatch $notePattern) { return $null }
        $suffix = $suffix.Substring(0, $slash)
    }
    if ($types -ccontains $suffix.Trim($spaces)) { return 'chord' }
    return $null
}

# A key: its pitch and scale as text, or $null when it's wrong.
function Read-Key($key, [string]$where) {
    if ($key -isnot [pscustomobject]) { $problems.Add("$where needs a key, like { ""root"": ""A"", ""scale"": ""naturalMinor"" }."); return $null }
    $root = if ($key.root -is [string]) { $key.root.Trim($spaces) } else { '' }
    $good = $true
    if ($root -cnotmatch $notePattern) { $problems.Add("$where has the key root '$($key.root)'. Write a letter A to G, then # or b when it needs one, like F# or Bb."); $good = $false }
    if ($key.scale -isnot [string] -or $key.scale -cnotin $scales) { $problems.Add("$where has the scale '$($key.scale)'. Use one of: $($scales -join ', ')."); $good = $false }
    if ($good) { return "$(Get-Pitch $root) $($key.scale)" }
    return $null
}

if ($song.format -cne 'windows-midi-sequencer-song') { $problems.Add('"format" has to be "windows-midi-sequencer-song".') }
$version = Get-Whole $song.version
if ($null -ne $version -and $version -gt 1) { $problems.Add("This answer is version $version, made for a newer version of Windows MIDI Sequencer. Write version 1.") }
elseif ($version -ne 1) { $problems.Add('"version" has to be the number 1.') }

$meterText = $null
if ($null -ne $song.meter) {
    $meter = [regex]::Match([string]$song.meter, '^(\d{1,2})/(\d{1,2})$')
    if (-not $meter.Success -or [int]$meter.Groups[1].Value -notin 1..32 -or [int]$meter.Groups[2].Value -notin 1, 2, 4, 8, 16, 32) {
        $problems.Add("""meter"" is '$($song.meter)'. Write it like ""4/4"", ""3/4"" or ""6/8"", or leave it out.")
    }
    else {
        $meterText = $meter.Value
        $top = [int]$meter.Groups[1].Value
        if ($BeatsPerBar -gt 0 -and $BeatsPerBar -ne $top) { $problems.Add("The answer is in $meterText, but -BeatsPerBar is $BeatsPerBar. The meter has to match the sequence's.") }
        elseif ($BeatsPerBar -le 0) { $BeatsPerBar = $top }
    }
}
if ($BeatsPerBar -le 0) { $BeatsPerBar = 4 }
if ($null -eq $meterText) { $meterText = "$BeatsPerBar beats a bar" }

$lastKey = Read-Key $song.key 'The song'
$keyChanges = 0
$totalBars = 0
$totalChords = 0
$chordCount = 0
$sections = if ($song.sections -is [array]) { $song.sections } else { @() }

if ($song.sections -isnot [array] -or $sections.Count -eq 0) { $problems.Add('"sections" has to be a list with at least one section.') }
elseif ($sections.Count -gt 256) { $problems.Add("The song has $($sections.Count) sections. The most is 256.") }

for ($i = 0; $i -lt $sections.Count -and $sections.Count -le 256; $i++) {
    $section = $sections[$i]
    if ($section -isnot [pscustomobject]) { $problems.Add("Section $($i + 1) is empty. Each section is an object with a name, bars and chords."); continue }
    $name = if ($section.name -is [string]) { $section.name.Trim($spaces) } else { '' }
    $where = if ($name) { $name } else { "Section $($i + 1)" }
    if ($name.Length -lt 1 -or $name.Length -gt 40) { $problems.Add("Section $($i + 1) needs a name of 1 to 40 characters.") }

    $bars = Get-Whole $section.bars
    if ($null -eq $bars -or $bars -lt 1 -or $bars -gt 999) { $problems.Add("$where has '$($section.bars)' bars. Write a whole number from 1 to 999."); $bars = $null }
    else { $totalBars += $bars }

    if ($null -ne $section.key) {
        $key = Read-Key $section.key $where
        if ($null -ne $key -and $null -ne $lastKey -and $key -ne $lastKey) { $keyChanges++ }
        if ($null -ne $key) { $lastKey = $key }
    }

    $chords = if ($section.chords -is [array]) { $section.chords } else { @() }
    if ($section.chords -isnot [array] -or $chords.Count -eq 0) { $problems.Add("$where has no chords. Give it at least one, or ""N.C."" for silence."); continue }
    $beats = 0
    $beatsGood = $true
    foreach ($entry in $chords) {
        $totalChords++
        if ($entry -isnot [pscustomobject]) { $problems.Add("${where} has an empty chord. Each chord is an object with a chord and beats."); $beatsGood = $false; continue }
        $kind = if ($entry.chord -is [string]) { Read-Chord $entry.chord } else { $null }
        if ($null -eq $kind) { $problems.Add("${where}: the app can't read the chord '$($entry.chord)'. Use a type from the list in the guide.") }
        elseif ($kind -eq 'chord') { $chordCount++ }
        $length = Get-Whole $entry.beats
        if ($null -eq $length -or $length -lt 1) { $problems.Add("${where}: the chord '$($entry.chord)' has '$($entry.beats)' beats. Write a whole number, 1 or more."); $beatsGood = $false }
        else { $beats += $length }
    }
    if ($beatsGood -and $null -ne $bars -and $beats -ne $bars * $BeatsPerBar) {
        $problems.Add("$where has $beats beats of chords but is $bars bars of $meterText, which is $($bars * $BeatsPerBar).")
    }
}

if ($totalBars -gt 999) { $problems.Add("The song is $totalBars bars long. The most is 999.") }
if ($totalChords -gt 4096) { $problems.Add("The song has $totalChords chords. The most is 4,096.") }

$warnings
if ($problems.Count -gt 0) {
    $problems
    if ($problems.Count -eq 1) { '1 problem' } else { "$($problems.Count) problems" }
    exit 1
}

$plural = { param($n, $word) if ($n -eq 1) { "$n $word" } else { "$n ${word}s" } }
"OK: $(& $plural $sections.Count 'section'), $(& $plural $totalBars 'bar'), $(& $plural $chordCount 'chord'), $(& $plural $keyChanges 'key change')"
exit 0
```

A plan that passes prints a line like `OK: 8 sections, 64 bars, 31 chords, 1 key change`. "N.C." isn't counted as a chord, and a key change is only counted when the key is different from the one before. Otherwise it prints one line for each problem, then how many there are. A line that starts with `Warning:` isn't a problem, but fix it anyway. The script can't see the sequence, so it can't tell whether the song crosses a meter change.

A plan that passes can still be the wrong song. Only the person can tell you that.

### 5. Show the person the plan

Before the code block, show the person the plan in plain words, and ask them to confirm it. For example:

| Section | Bars | Chords |
| --- | --- | --- |
| Intro | 4 | Am, F, C, G |
| Verse | 8 | Am, F, C, G, Am, F, C, G |
| Chorus | 8 | C, G, Am, F, C, G, F, F |

If they want changes, make them and write the whole plan again.

### 6. Hand it over

Tell the person how to put the plan into their sequence:

1. Copy your whole answer. Windows MIDI Sequencer finds the plan inside it.
2. In the **Ask an AI assistant…** dialog, go to the **Answer** step and paste it into the box.
3. Select **Check**. The dialog shows what's in the plan, such as "8 sections, 64 bars, 31 chords, 1 key change", or says plainly what's wrong. If something's wrong, paste that message back to you so you can fix it.
4. Pick how to apply it. **Replace** puts the song at bar 1, in place of the row's keys and chords and the tags on **Tempo and meter**. **Add at playhead** puts it at the start of the bar where the playhead is, and keeps everything else.
5. Select **Apply**. It's one step to undo.

Then tell them what they can do next, since the plan has no notes:

- Right-click the row and turn on **Play chords** to hear the chords.
- Right-click the row and select **Copy chords to new track** to turn the chords into notes they can edit.
- Set a generator's key to **Song key**, so its melody follows the key, wherever it changes.
- Set an arpeggiator's notes to come from the **Key and chords row**.
- Add a **Strum** generator, which strums the chords on the row like a guitar, a ukulele or a piano, on a pattern they pick.

Also tell them what they asked for that the plan can't do, and what you did instead.

## What the app does with the plan

When the person selects **Apply**, Windows MIDI Sequencer:

- Puts the song's `key` on the **Key and chords** row where the song starts, and each section's `key` where that section starts.
- Puts each chord on the row, one after another, each for its beats. "N.C." leaves those beats empty.
- Adds a tag on **Tempo and meter** at the first bar of each section, named after the section. Intro, Verse, Pre-chorus, Chorus, Bridge, Solo, Breakdown, and Outro each get a color of their own. It goes by how the name starts, so Verse 2 gets the Verse color. Any other name gets a color picked from the name, and the same name always gets the same color.
- With **Replace**, starts the song at bar 1. The row's keys and chords and the tags on **Tempo and meter** are replaced.
- With **Add at playhead**, starts the song at the start of the bar the playhead is in. Chords already on the row where the new ones land are cut short or replaced, and the new tags are added to the ones already there.
- Makes all of it one step to undo.

Nothing is half applied. If any check fails, nothing changes, and the dialog says why.

## What Windows MIDI Sequencer can't do {#what-sequencer-cant-do}

Tell the person about these before they find out on their own.

- **A song plan has no notes, drums, clips, or loops.** It writes only the key, the chords, and the sections. The person can turn the chords into music with **Copy chords to new track**, **Play chords**, generators set to **Song key**, the arpeggiator reading the **Key and chords row**, and a **Strum** generator, which strums the row's chords.
- **No tempo changes.** The song plays at the tempo the sequence already has. A `tempo` in the plan is ignored. The person changes tempo on **Tempo and meter**.
- **No meter changes inside the song.** The whole plan uses one meter, and it has to be the sequence's meter where the song goes. The song also can't run across a place where the sequence changes meter. To put a 3/4 bridge in a 4/4 song, write the parts before and after the bridge as their own plans in 4/4, and the bridge as a plan in 3/4. The person adds each one at the playhead, starting at the bar where it goes.
- **No lyrics.** Windows MIDI Sequencer has lyrics, but a song plan can't write them.
- **Only the chord types in [Chord types](#chord-types).** Chords like C7♯9, C13♭9, CmMaj7, or a C5 power chord can't be written. Use the nearest type on the list.
- **Only whole beats.** A chord can't change on an "and", such as half way through a beat in 4/4.

## The song plan

### A complete example

This plan has a two-bar intro in A minor and a two-bar chorus in C major that ends with two beats of silence. It's in 4/4.

```midi-sequencer-song
{
  "format": "windows-midi-sequencer-song",
  "version": 1,
  "meter": "4/4",
  "key": { "root": "A", "scale": "naturalMinor" },
  "sections": [
    { "name": "Intro", "bars": 2, "chords": [ { "chord": "Am", "beats": 4 }, { "chord": "F", "beats": 4 } ] },
    { "name": "Chorus", "bars": 2, "key": { "root": "C", "scale": "major" },
      "chords": [ { "chord": "C", "beats": 4 }, { "chord": "G/B", "beats": 2 }, { "chord": "N.C.", "beats": 2 } ] }
  ]
}
```

Each section is 2 bars of 4 beats, so its chords add up to 8 beats: 4 + 4 in the intro, and 4 + 2 + 2 in the chorus. The checking script says `OK: 2 sections, 4 bars, 4 chords, 1 key change`.

### Where the plan goes in the answer

- Put the plan in one code block whose first line is <code>```midi-sequencer-song</code>. Anything outside it is ignored, so your summary and your notes can go around it.
- Windows MIDI Sequencer also finds a plan in a <code>```json</code> block, or a JSON object pasted on its own, but use the `midi-sequencer-song` fence. It's the one the app looks for first.
- If an answer has more than one `midi-sequencer-song` block, the app reads only the first.

### The top level

| Key | Needed | What it holds |
| --- | --- | --- |
| `format` | Yes | Exactly `"windows-midi-sequencer-song"`. |
| `version` | Yes | The number `1`. A higher number is refused as made for a newer version of the app. |
| `meter` | No | The meter as text, like `"4/4"`, `"3/4"`, or `"6/8"`: a top number from 1 to 32, a slash, and a bottom number of 1, 2, 4, 8, 16, or 32. It has to match the sequence's meter where the song goes, or the app refuses the plan and says why. Leave it out, or write `null`, to use the sequence's meter. |
| `key` | Yes | The key at the start of the song. See [Keys](#keys). |
| `sections` | Yes | A list of 1 to 256 sections, in the order they play. See [Sections](#sections). |

Any other key is ignored. A `title`, a `tempo`, or `notes` does no harm, but does nothing.

### Keys

A key is an object with a `root` and a `scale`: `{ "root": "A", "scale": "naturalMinor" }`.

**Notes.** A key's `root`, a chord's root, and a chord's bass note are each a capital letter A to G, then one sharp or one flat if it needs one. A sharp is `#` or `♯`. A flat is `b` or `♭`. For example: `"C"`, `"F#"`, `"Bb"`, `"E♭"`. No double sharps or flats, and no words like "flat".

**Scales.** The `scale` is one of these names, spelled exactly like this, capital letters included:

| `scale` | The scale |
| --- | --- |
| `major` | Major |
| `naturalMinor` | Natural minor |
| `dorian` | Dorian |
| `phrygian` | Phrygian |
| `lydian` | Lydian |
| `mixolydian` | Mixolydian |
| `harmonicMinor` | Harmonic minor |
| `majorPentatonic` | Major pentatonic |
| `minorPentatonic` | Minor pentatonic |
| `blues` | Blues |
| `chromatic` | Chromatic |

### Sections

| Key | Needed | What it holds |
| --- | --- | --- |
| `name` | Yes | The section's name, 1 to 40 characters once spaces at the start and end are taken off. It becomes the tag's text. |
| `bars` | Yes | How long the section is, as a whole number of bars from 1 to 999. `8.0` is read as 8. |
| `key` | No | A key change where this section starts. It lasts until the next section that has a `key`. Leave it out, or write `null`, to stay in the key before. |
| `chords` | Yes | A list of at least one chord, in the order they play. See [Chords](#chords). |

The whole song can have at most 999 bars.

### Chords

Each chord is an object: `{ "chord": "Dm7", "beats": 4 }`.

| Key | Needed | What it holds |
| --- | --- | --- |
| `chord` | Yes | The chord's name: a root note, a type from [Chord types](#chord-types), and, if it needs one, a slash and a bass note. For example `"Dm7"`, `"C/E"`, `"B♭maj7"`, `"F#m7b5"`, `"Gsus4"`, `"E7/G#"`. Or `"N.C."` for no chord. Spaces at the start and end are ignored. |
| `beats` | Yes | How long the chord lasts, as a whole number of beats, 1 or more. `4.0` is read as 4. |

A section's chords fill it exactly: their `beats` add up to its `bars` times the beats in a bar. A chord can't run over into the next section. Write the same chord again at the start of the next section instead.

The whole song can have at most 4,096 chords.

### Chord types

Write the type right after the root note. Use the names in the first column. The app also reads the other names listed, but stick to the first column so the person sees the names they expect.

| Type | Example | Write | The app also reads |
| --- | --- | --- | --- |
| Major | C | nothing | `maj`, `M` |
| Minor | Cm | `m` | `min`, `-` |
| Seventh | C7 | `7` | |
| Major seventh | Cmaj7 | `maj7` | `M7` |
| Minor seventh | Cm7 | `m7` | `min7`, `-7` |
| Diminished | Cdim | `dim` | `°` |
| Suspended fourth | Csus4 | `sus4` | `sus` |
| Suspended second | Csus2 | `sus2` | |
| Sixth | C6 | `6` | |
| Minor sixth | Cm6 | `m6` | `min6`, `-6` |
| Ninth | C9 | `9` | |
| Major ninth | Cmaj9 | `maj9` | `M9` |
| Minor ninth | Cm9 | `m9` | `min9`, `-9` |
| Added ninth | Cadd9 | `add9` | |
| Augmented | Caug | `aug` | `+` |
| Diminished seventh | Cdim7 | `dim7` | `°7` |
| Half-diminished | Cm7♭5 | `m7♭5` or `m7b5` | `min7b5`, `-7b5`, `ø`, `ø7` |
| Seventh, suspended fourth | C7sus4 | `7sus4` | `7sus` |
| Eleventh | C11 | `11` | |
| Thirteenth | C13 | `13` | |

Capital letters matter: `M7` is a major seventh, and `m7` is a minor seventh. Any other type is refused, such as `7#9`, `13b9`, `mMaj7`, `5`, or `maj7#11`. Use the nearest type on the list:

| Instead of | Write |
| --- | --- |
| `7#9`, `7b9`, `7#5`, `7alt` | `7` |
| `13b9`, `13#11` | `13` |
| `maj7#11`, `maj13` | `maj7` or `maj9` |
| `mMaj7`, `m(maj7)` | `m` |
| `5` (a power chord) | nothing, for a major chord |
| `m11`, `m13` | `m9` |
| `6/9` | `6` |

"N.C." means no chord. The app also reads `N.C` and `NC`. It isn't counted as a chord in the summary.

## The checks the app makes

When the person selects **Check**, Windows MIDI Sequencer reads the plan and refuses it, with a plain reason, if:

- There's no JSON object in the answer, or it isn't strict JSON.
- `format` isn't `"windows-midi-sequencer-song"`.
- `version` isn't the number 1. A higher number is refused as made for a newer version.
- `meter` isn't written like `"4/4"`, or doesn't match the sequence's meter where the song goes.
- The sequence changes meter inside the song. The song's bars, counted from where it goes, run across a meter change on **Tempo and meter**.
- The song's `key`, or a section's `key`, is missing a root or a scale, or has one the app doesn't know.
- There are no sections, or more than 256, or an entry in `sections` or in a section's `chords` is `null` instead of an object.
- A section's `name` is empty or longer than 40 characters.
- A section's `bars` isn't a whole number from 1 to 999, or the whole song is longer than 999 bars.
- A section has no chords, or the whole song has more than 4,096.
- A chord's name can't be read, or its `beats` isn't a whole number of 1 or more.
- A section's chords don't fill it exactly. For example: "Chorus has 36 beats of chords but is 8 bars of 4/4, which is 32."

The app doesn't check that the chords sound good, or that they suit the key.

## Mistakes that are easy to miss

> **For agents:** Windows MIDI Sequencer refuses a plan with any of these, and nothing goes on the row until it's fixed. The one exception is more than one block, where the app uses the first.
>
> - A section's chords don't add up to its bars times the beats in a bar. Count them again for every section, especially after changing a section's length.
> - A beat in 6/8 is counted as a dotted quarter. In 6/8 a bar has 6 beats, so 4 bars is 24 beats.
> - The plan's `meter` is different from the sequence's meter in the prompt. Use the sequence's meter, or leave `meter` out.
> - A sharp or a flat is written as a word, like `"B flat"` or `"F sharp"`. Write `"Bb"` or `"F#"`.
> - A note is written in lower case, like `"a"` or `"bb"`. The letter is always a capital.
> - A scale is written the way people say it, like `"Natural minor"`, `"natural minor"`, or `"minor"`. Write `"naturalMinor"`, exactly.
> - A chord type isn't on the list, like `C7#9`, `CmMaj7`, `C5`, or `Cm11`. Use the nearest type on the list.
> - `M7` and `m7` are mixed up. `M7` is a major seventh.
> - A chord is half a beat long, like `"beats": 1.5`. Beats are whole numbers.
> - The plan has a `tempo`, notes, drums, or lyrics, and the person expects them to do something. They're ignored.
> - The answer has more than one `midi-sequencer-song` block, such as a first draft and a fixed one. The app uses the first block and ignores the rest, so the fixed one is never read. Write one block, with the whole song.
> - The song runs across a meter change that the prompt listed, such as "From bar 33 it's in 3/4". Keep the song inside one meter, or ask the person where it should go.
> - A list has a `null` in it, such as `"chords": [ { "chord": "C", "beats": 4 }, null ]`. Every entry in `sections` and `chords` is an object.
> - `format` or `version` is left out, so the plan is refused.
> - The JSON has a comment, or a comma after the last item in a list or an object.
> - A section has an empty name, or a name longer than 40 characters, such as a description of the section instead of its name.
