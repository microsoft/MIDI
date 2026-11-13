# Copyright (c) Microsoft Corporation and Contributors.
# Licensed under the MIT License
# ============================================================================
# This is part of Windows MIDI Services
# Further information: https://aka.ms/midi
# ============================================================================
#
# Writes the Standard MIDI Files the sequencing tests read. The files are checked in, so this
# only needs running when a fixture changes or a new one is wanted. Everything it writes is
# deliberate: the tests assert exact counts, so changing a file here means changing a test.
#
# Files are kept SHORT on purpose. Several tests play them for real, and nobody should have to
# sit through a five minute file to find out whether a stop worked.

[CmdletBinding()]
param(
    [string] $OutputFolder = (Join-Path $PSScriptRoot 'files')
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $OutputFolder)) {
    New-Item -ItemType Directory -Path $OutputFolder -Force | Out-Null
}

$TicksPerQuarter = 480

function New-Vlq {
    param([uint32] $Value)

    $stack = New-Object System.Collections.Generic.Stack[byte]
    $stack.Push([byte]($Value -band 0x7F))
    $Value = [uint32]($Value -shr 7)

    while ($Value -gt 0) {
        $stack.Push([byte](($Value -band 0x7F) -bor 0x80))
        $Value = [uint32]($Value -shr 7)
    }

    # No comma here: callers iterate the bytes, so unrolling is what is wanted.
    return @($stack.ToArray())
}

function Add-Event {
    param(
        [System.Collections.Generic.List[byte]] $Track,
        [uint32] $Delta,
        [byte[]] $Bytes
    )

    foreach ($b in (New-Vlq $Delta)) { $Track.Add($b) }
    foreach ($b in $Bytes) { $Track.Add($b) }
}

function Add-Meta {
    param(
        [System.Collections.Generic.List[byte]] $Track,
        [uint32] $Delta,
        [byte] $MetaType,
        [byte[]] $Payload
    )

    $bytes = New-Object System.Collections.Generic.List[byte]
    $bytes.Add(0xFF)
    $bytes.Add($MetaType)
    foreach ($b in (New-Vlq ([uint32]$Payload.Length))) { $bytes.Add($b) }
    foreach ($b in $Payload) { $bytes.Add($b) }

    Add-Event -Track $Track -Delta $Delta -Bytes $bytes.ToArray()
}

function Add-MetaText {
    param(
        [System.Collections.Generic.List[byte]] $Track,
        [uint32] $Delta,
        [byte] $MetaType,
        [string] $Text
    )

    Add-Meta -Track $Track -Delta $Delta -MetaType $MetaType -Payload ([System.Text.Encoding]::UTF8.GetBytes($Text))
}

function Add-Tempo {
    param(
        [System.Collections.Generic.List[byte]] $Track,
        [uint32] $Delta,
        [uint32] $MicrosecondsPerQuarter
    )

    Add-Meta -Track $Track -Delta $Delta -MetaType 0x51 -Payload @(
        [byte](($MicrosecondsPerQuarter -shr 16) -band 0xFF),
        [byte](($MicrosecondsPerQuarter -shr 8) -band 0xFF),
        [byte]($MicrosecondsPerQuarter -band 0xFF))
}

function Add-TimeSignature {
    param(
        [System.Collections.Generic.List[byte]] $Track,
        [uint32] $Delta,
        [byte] $Numerator,
        [byte] $DenominatorPowerOfTwo
    )

    Add-Meta -Track $Track -Delta $Delta -MetaType 0x58 -Payload @($Numerator, $DenominatorPowerOfTwo, 24, 8)
}

function Add-EndOfTrack {
    param([System.Collections.Generic.List[byte]] $Track, [uint32] $Delta = 0)
    Add-Meta -Track $Track -Delta $Delta -MetaType 0x2F -Payload @()
}

# The chord symbol carried by a manufacturer system exclusive, which is how lead sheet and
# karaoke files store chords: F0 <len> 00 20 24 00 01 <ascii> 0A F7
function Add-ChordSymbol {
    param(
        [System.Collections.Generic.List[byte]] $Track,
        [uint32] $Delta,
        [string] $Name
    )

    $payload = New-Object System.Collections.Generic.List[byte]
    foreach ($b in @(0x00, 0x20, 0x24, 0x00, 0x01)) { $payload.Add([byte]$b) }
    foreach ($c in [System.Text.Encoding]::ASCII.GetBytes($Name)) { $payload.Add($c) }
    $payload.Add(0x0A)
    $payload.Add(0xF7)

    $bytes = New-Object System.Collections.Generic.List[byte]
    $bytes.Add(0xF0)
    foreach ($b in (New-Vlq ([uint32]$payload.Count))) { $bytes.Add($b) }
    foreach ($b in $payload) { $bytes.Add($b) }

    Add-Event -Track $Track -Delta $Delta -Bytes $bytes.ToArray()
}

function Write-MidiFile {
    param(
        [string] $Path,
        [uint16] $Format,
        [object] $Tracks,
        [uint16] $Division = $TicksPerQuarter
    )

    # Passed either one track list or a list of them. Wrapping in @() would enumerate a single
    # track's bytes and turn each one into a track of its own.
    $trackList = New-Object System.Collections.Generic.List[object]

    if ($Tracks -is [System.Collections.Generic.List[byte]]) {
        $trackList.Add($Tracks)
    }
    else {
        foreach ($tr in $Tracks) { $trackList.Add($tr) }
    }

    $out = New-Object System.Collections.Generic.List[byte]

    foreach ($b in [System.Text.Encoding]::ASCII.GetBytes('MThd')) { $out.Add($b) }
    foreach ($b in @(0, 0, 0, 6)) { $out.Add([byte]$b) }
    $out.Add([byte](($Format -shr 8) -band 0xFF)); $out.Add([byte]($Format -band 0xFF))
    $out.Add([byte](($trackList.Count -shr 8) -band 0xFF)); $out.Add([byte]($trackList.Count -band 0xFF))
    $out.Add([byte](($Division -shr 8) -band 0xFF)); $out.Add([byte]($Division -band 0xFF))

    foreach ($track in $trackList) {
        foreach ($b in [System.Text.Encoding]::ASCII.GetBytes('MTrk')) { $out.Add($b) }
        $len = $track.Count
        $out.Add([byte](($len -shr 24) -band 0xFF))
        $out.Add([byte](($len -shr 16) -band 0xFF))
        $out.Add([byte](($len -shr 8) -band 0xFF))
        $out.Add([byte]($len -band 0xFF))
        foreach ($b in $track) { $out.Add($b) }
    }

    [System.IO.File]::WriteAllBytes($Path, $out.ToArray())
    Write-Host ("  {0,-34} {1,7} bytes" -f (Split-Path $Path -Leaf), $out.Count)
}

# The comma is load bearing: returning a list from a PowerShell function otherwise enumerates it,
# and an empty track would come back as null.
function New-Track { $list = New-Object System.Collections.Generic.List[byte]; return , $list }

Write-Host "Writing test files to $OutputFolder"

# ---------------------------------------------------------------------------------------------
# plain-scale.mid
# The simplest useful file: one track, one tempo, 4/4, eight paired notes. About 2 seconds.
# ---------------------------------------------------------------------------------------------
$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'Scale'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $t -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
foreach ($i in 0..7) {
    Add-Event -Track $t -Delta 0 -Bytes @(0x90, [byte](60 + $i), 100)
    Add-Event -Track $t -Delta 240 -Bytes @(0x80, [byte](60 + $i), 0)
}
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'plain-scale.mid') -Format 0 -Tracks $t

# ---------------------------------------------------------------------------------------------
# multi-track.mid
# Format 1, three tracks on three channels, for routing, mute and solo tests.
# ---------------------------------------------------------------------------------------------
$conductor = New-Track
Add-MetaText -Track $conductor -Delta 0 -MetaType 0x03 -Text 'Conductor'
Add-Tempo -Track $conductor -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $conductor -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
Add-Tempo -Track $conductor -Delta 1920 -MicrosecondsPerQuarter 300000
Add-EndOfTrack -Track $conductor -Delta 960

$tracks = New-Object System.Collections.Generic.List[object]
$tracks.Add($conductor)
foreach ($channel in 0..2) {
    $tr = New-Track
    Add-MetaText -Track $tr -Delta 0 -MetaType 0x03 -Text ("Part {0}" -f ($channel + 1))
    Add-MetaText -Track $tr -Delta 0 -MetaType 0x04 -Text 'Acoustic Grand Piano'
    Add-Event -Track $tr -Delta 0 -Bytes @([byte](0xC0 -bor $channel), [byte]($channel * 8))
    foreach ($i in 0..5) {
        Add-Event -Track $tr -Delta 0 -Bytes @([byte](0x90 -bor $channel), [byte](48 + $channel * 7 + $i), 100)
        Add-Event -Track $tr -Delta 240 -Bytes @([byte](0x80 -bor $channel), [byte](48 + $channel * 7 + $i), 0)
    }
    Add-EndOfTrack -Track $tr
    $tracks.Add($tr)
}
Write-MidiFile -Path (Join-Path $OutputFolder 'multi-track.mid') -Format 1 -Tracks $tracks

# ---------------------------------------------------------------------------------------------
# with-lyrics.mid  /  without-lyrics.mid
# Lyric events with explicit line breaks, and the same music with none.
# ---------------------------------------------------------------------------------------------
$syllables = @('Row', ' row', ' row', ' your', ' boat', '/Gent', 'ly', ' down', ' the', ' stream')

$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'With lyrics'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $t -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
foreach ($i in 0..($syllables.Count - 1)) {
    Add-MetaText -Track $t -Delta 0 -MetaType 0x05 -Text $syllables[$i]
    Add-Event -Track $t -Delta 0 -Bytes @(0x90, [byte](60 + ($i % 5)), 100)
    Add-Event -Track $t -Delta 200 -Bytes @(0x80, [byte](60 + ($i % 5)), 0)
}
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'with-lyrics.mid') -Format 0 -Tracks $t

$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'Without lyrics'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $t -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
foreach ($i in 0..($syllables.Count - 1)) {
    Add-Event -Track $t -Delta 0 -Bytes @(0x90, [byte](60 + ($i % 5)), 100)
    Add-Event -Track $t -Delta 200 -Bytes @(0x80, [byte](60 + ($i % 5)), 0)
}
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'without-lyrics.mid') -Format 0 -Tracks $t

# ---------------------------------------------------------------------------------------------
# soft-karaoke.mid
# The .kar convention: words in plain TEXT events, announced with an @K tag.
# ---------------------------------------------------------------------------------------------
$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x01 -Text '@KMIDI KARAOKE FILE'
Add-MetaText -Track $t -Delta 0 -MetaType 0x01 -Text '@TTest Song'
Add-MetaText -Track $t -Delta 0 -MetaType 0x01 -Text '@LENGL'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $t -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
$karaoke = @('\Twin', 'kle', ' twin', 'kle', '/lit', 'tle', ' star')
foreach ($i in 0..($karaoke.Count - 1)) {
    Add-MetaText -Track $t -Delta 0 -MetaType 0x01 -Text $karaoke[$i]
    Add-Event -Track $t -Delta 0 -Bytes @(0x90, [byte](60 + ($i % 5)), 100)
    Add-Event -Track $t -Delta 200 -Bytes @(0x80, [byte](60 + ($i % 5)), 0)
}
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'soft-karaoke.mid') -Format 0 -Tracks $t

# ---------------------------------------------------------------------------------------------
# with-chords.mid  /  without-chords.mid
# ---------------------------------------------------------------------------------------------
$chords = @('C', 'Am7', 'F', 'G7')

$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'With chords'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $t -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
foreach ($i in 0..($chords.Count - 1)) {
    Add-ChordSymbol -Track $t -Delta $(if ($i -eq 0) { 0 } else { 0 }) -Name $chords[$i]
    Add-Event -Track $t -Delta 0 -Bytes @(0x90, [byte](60 + $i * 2), 100)
    Add-Event -Track $t -Delta 480 -Bytes @(0x80, [byte](60 + $i * 2), 0)
}
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'with-chords.mid') -Format 0 -Tracks $t

$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'Without chords'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $t -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
foreach ($i in 0..($chords.Count - 1)) {
    Add-Event -Track $t -Delta 0 -Bytes @(0x90, [byte](60 + $i * 2), 100)
    Add-Event -Track $t -Delta 480 -Bytes @(0x80, [byte](60 + $i * 2), 0)
}
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'without-chords.mid') -Format 0 -Tracks $t

# ---------------------------------------------------------------------------------------------
# meter-changes.mid
# 4/4 then 3/4 then 7/8, for bar and beat arithmetic.
# ---------------------------------------------------------------------------------------------
$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'Meter changes'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $t -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 60, 100)
Add-Event -Track $t -Delta 1920 -Bytes @(0x80, 60, 0)
Add-TimeSignature -Track $t -Delta 0 -Numerator 3 -DenominatorPowerOfTwo 2
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 62, 100)
Add-Event -Track $t -Delta 1440 -Bytes @(0x80, 62, 0)
Add-TimeSignature -Track $t -Delta 0 -Numerator 7 -DenominatorPowerOfTwo 3
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 64, 100)
Add-Event -Track $t -Delta 1680 -Bytes @(0x80, 64, 0)
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'meter-changes.mid') -Format 0 -Tracks $t

# ---------------------------------------------------------------------------------------------
# hanging-notes.mid
# Sustain pedal down and four notes that are never released. The file ends with them sounding,
# which is what the end-of-sequence silencing has to cope with.
# ---------------------------------------------------------------------------------------------
$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'Hanging notes'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $t -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
foreach ($i in 0..3) {
    Add-Event -Track $t -Delta 0 -Bytes @(0x90, [byte](60 + $i), 100)
    Add-Event -Track $t -Delta 120 -Bytes @(0x80, [byte](60 + $i), 0)
}
Add-Event -Track $t -Delta 0 -Bytes @(0xB0, 0x40, 127)
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 72, 100)
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 76, 100)
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 79, 100)
Add-Event -Track $t -Delta 0 -Bytes @(0x92, 48, 100)
Add-EndOfTrack -Track $t -Delta 480
Write-MidiFile -Path (Join-Path $OutputFolder 'hanging-notes.mid') -Format 0 -Tracks $t

# ---------------------------------------------------------------------------------------------
# black-midi.mid
# Dense content. Not a real "black MIDI" of millions of notes, which would make the repository
# heavier for no extra coverage, but dense enough that a per-note cost would show: 16 channels
# playing 40,000 short notes in about 4 seconds.
# ---------------------------------------------------------------------------------------------
$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'Dense'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-TimeSignature -Track $t -Delta 0 -Numerator 4 -DenominatorPowerOfTwo 2
$rand = New-Object System.Random 20260917
foreach ($step in 0..1249) {
    foreach ($voice in 0..15) {
        $note = [byte](24 + $rand.Next(0, 84))
        Add-Event -Track $t -Delta 0 -Bytes @([byte](0x90 -bor $voice), $note, 96)
        Add-Event -Track $t -Delta 0 -Bytes @([byte](0x80 -bor $voice), $note, 0)
    }
    Add-Event -Track $t -Delta 3 -Bytes @(0xB0, 0x0B, 100)
}
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'black-midi.mid') -Format 0 -Tracks $t

# ---------------------------------------------------------------------------------------------
# running-status.mid, system-messages.mid, long-sysex.mid
# Shapes the reader has to survive: running status, System Common and Real Time bytes in the
# middle of a track, and a system exclusive large enough to span many packets.
# ---------------------------------------------------------------------------------------------
$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'Running status'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 60, 100)
foreach ($i in 1..7) {
    Add-Event -Track $t -Delta 120 -Bytes @([byte](60 + $i), 100)      # running status, no status byte
}
foreach ($i in 0..7) {
    Add-Event -Track $t -Delta 60 -Bytes @(0x80, [byte](60 + $i), 0)
}
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'running-status.mid') -Format 0 -Tracks $t

$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'System messages'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 60, 100)
Add-Event -Track $t -Delta 60 -Bytes @(0xF8)                          # clock, no data bytes
Add-Event -Track $t -Delta 0 -Bytes @(0xFE)                           # active sensing
Add-Event -Track $t -Delta 60 -Bytes @(0xF1, 0x20)                    # quarter frame, one data byte
Add-Event -Track $t -Delta 0 -Bytes @(0xF3, 0x05)                     # song select, one data byte
Add-Event -Track $t -Delta 60 -Bytes @(0xF2, 0x10, 0x20)              # song position, two data bytes
Add-Event -Track $t -Delta 0 -Bytes @(0xF6)                           # tune request
Add-Event -Track $t -Delta 120 -Bytes @(0x80, 60, 0)
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'system-messages.mid') -Format 0 -Tracks $t

$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'Long system exclusive'
Add-Tempo -Track $t -Delta 0 -MicrosecondsPerQuarter 500000
$payload = New-Object System.Collections.Generic.List[byte]
foreach ($b in @(0x00, 0x21, 0x09)) { $payload.Add([byte]$b) }
foreach ($i in 0..1999) { $payload.Add([byte]($i % 128)) }
$payload.Add(0xF7)
$bytes = New-Object System.Collections.Generic.List[byte]
$bytes.Add(0xF0)
foreach ($b in (New-Vlq ([uint32]$payload.Count))) { $bytes.Add($b) }
foreach ($b in $payload) { $bytes.Add($b) }
Add-Event -Track $t -Delta 0 -Bytes $bytes.ToArray()
Add-Event -Track $t -Delta 240 -Bytes @(0x90, 60, 100)
Add-Event -Track $t -Delta 240 -Bytes @(0x80, 60, 0)
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'long-sysex.mid') -Format 0 -Tracks $t

# ---------------------------------------------------------------------------------------------
# empty-track.mid
# Reads cleanly and contains nothing to play. The reader should say so rather than fail.
# ---------------------------------------------------------------------------------------------
$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'Nothing to play'
Add-MetaText -Track $t -Delta 0 -MetaType 0x02 -Text 'Copyright test'
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $OutputFolder 'empty-track.mid') -Format 0 -Tracks $t

# ---------------------------------------------------------------------------------------------
# smpte-timing.mid
# Division is negative, so ticks are absolute frames and tempo does not apply.
# ---------------------------------------------------------------------------------------------
$t = New-Track
Add-MetaText -Track $t -Delta 0 -MetaType 0x03 -Text 'SMPTE'
foreach ($i in 0..3) {
    Add-Event -Track $t -Delta 0 -Bytes @(0x90, [byte](60 + $i), 100)
    Add-Event -Track $t -Delta 25 -Bytes @(0x80, [byte](60 + $i), 0)
}
Add-EndOfTrack -Track $t
# -25 frames per second, 40 subframes per frame
Write-MidiFile -Path (Join-Path $OutputFolder 'smpte-timing.mid') -Format 0 -Tracks $t -Division ([uint16]0xE728)

# ---------------------------------------------------------------------------------------------
# Malformed files. These exist to be refused or salvaged without crashing.
# ---------------------------------------------------------------------------------------------
$malformed = Join-Path $OutputFolder 'malformed'
if (-not (Test-Path $malformed)) { New-Item -ItemType Directory -Path $malformed -Force | Out-Null }

# not a MIDI file at all
[System.IO.File]::WriteAllBytes(
    (Join-Path $malformed 'not-a-midi-file.mid'),
    [System.Text.Encoding]::ASCII.GetBytes('This is plainly not a MIDI file, it is a sentence.'))

# a header and nothing else
[System.IO.File]::WriteAllBytes(
    (Join-Path $malformed 'header-only.mid'),
    ([byte[]](0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0)))

# truncated halfway through a track
$full = [System.IO.File]::ReadAllBytes((Join-Path $OutputFolder 'plain-scale.mid'))
[System.IO.File]::WriteAllBytes(
    (Join-Path $malformed 'truncated.mid'),
    $full[0..([int]($full.Length / 2))])

# a track chunk that claims far more bytes than the file holds
$t = New-Track
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 60, 100)
Add-Event -Track $t -Delta 240 -Bytes @(0x80, 60, 0)
$out = New-Object System.Collections.Generic.List[byte]
foreach ($b in [System.Text.Encoding]::ASCII.GetBytes('MThd')) { $out.Add($b) }
foreach ($b in @(0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0)) { $out.Add([byte]$b) }
foreach ($b in [System.Text.Encoding]::ASCII.GetBytes('MTrk')) { $out.Add($b) }
foreach ($b in @(0x7F, 0xFF, 0xFF, 0xFF)) { $out.Add([byte]$b) }       # absurd declared length
foreach ($b in $t) { $out.Add($b) }
[System.IO.File]::WriteAllBytes((Join-Path $malformed 'lying-track-length.mid'), $out.ToArray())
Write-Host ("  {0,-34} {1,7} bytes" -f 'malformed\lying-track-length.mid', $out.Count)

# a header claiming thousands of tracks that are not there
$out = New-Object System.Collections.Generic.List[byte]
foreach ($b in [System.Text.Encoding]::ASCII.GetBytes('MThd')) { $out.Add($b) }
foreach ($b in @(0, 0, 0, 6, 0, 1, 0xFF, 0xFF, 0x01, 0xE0)) { $out.Add([byte]$b) }
[System.IO.File]::WriteAllBytes((Join-Path $malformed 'lying-track-count.mid'), $out.ToArray())

# a note on with no matching note off and an impossible delta time
$t = New-Track
Add-Event -Track $t -Delta 0 -Bytes @(0x90, 60, 100)
foreach ($b in @(0xFF, 0xFF, 0xFF, 0x7F)) { $t.Add([byte]$b) }        # ~268 million tick delta
Add-Event -Track $t -Delta 0 -Bytes @(0x80, 60, 0)
Add-EndOfTrack -Track $t
Write-MidiFile -Path (Join-Path $malformed 'absurd-delta.mid') -Format 0 -Tracks $t

Write-Host ''
Write-Host ("Done. {0} files." -f (Get-ChildItem $OutputFolder -Recurse -Filter '*.mid').Count)
