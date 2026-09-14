# MIDI synthesizer prototype

Exploratory work toward a modern replacement for the in-box Microsoft GS Wavetable Synth, usable
from Windows MIDI Services. Nothing here ships. Nothing here is wired into the service.

## Why this shape

`midisrv` runs as `NT AUTHORITY\LocalService` in session 0. Audio endpoints, per-application
volume, device preferences and ASIO are all per-session or per-user concepts, and a faulty audio
driver taking down the service would take MIDI down for the whole machine. So the intended
architecture separates the two concerns:

- a service transport owns the endpoint, so it is always enumerable and has a stable device id
- a per-session render host process does the synthesis and the audio output, isolated from the
  service

Everything that matters lives in `MidiSynthLib`, which has no dependency on COM, on the service,
or on the SDK. The host is a thin shim. That way the hosting decision can change without
rewriting the engine, and the whole library can be exercised from a console application.

## Layout

| Path | What it is |
| --- | --- |
| `lib/MidiSynthLib` | Host-agnostic static library. DLS parsing and the voice engine. |
| `spikes/synthspike-dls` | Console spike. Parses a DLS collection and reports what is in it. |
| `spikes/synthspike-render` | Console spike. Renders a score offline to WAV, plus tuning, CPU and A/B tests. |
| `spikes/synthspike-capture` | Console spike. Captures the in-box synth via WASAPI loopback. |
| `spikes/synthspike-play` | Console spike. Plays the synth live through WASAPI from MIDI or a score. |
| `tools/specextract` | Extracts plain text from a specification PDF so it can be searched. |

## How close are we to the in-box synth

Measured by playing `spikes/synthspike-capture/compare-score.txt` through the in-box synth,
capturing it, and rendering the same score ourselves at the same rate. Level matched, both
natively 96 kHz so neither side went through a resampler:

| Band | Delta |
| --- | --- |
| 31 - 125 Hz | -1.5 to -1.1 dB |
| 125 Hz - 4 kHz | 0.0 to +0.4 dB |
| 4 - 16 kHz | -0.6 to +0.4 dB |

Absolute output level matches to within about 0.7 dB, which is what `MasterGainDb` is set for. A
drop-in replacement that is louder or quieter than what it replaces changes how every existing
MIDI file sounds.

Velocity response was checked separately with `velocity-sweep-score.txt`: the curve **matches
within 0.17 dB for velocities 24 to 127**, widening to 1.4 dB at velocity 8 where the note is
70 dB down and inaudible. Velocity 1 produces no output at all in either synth. Both track the
DLS concave transform, `40 * log10(velocity / 127)`.

### Open compatibility gaps

**Close enough for now.** These are recorded so the work is not lost, not because anything is
blocked on them.

Channel volume was suspected and cleared: the in-box synth applies the **same DLS concave transform**
to CC7 that we do, matching within 0.08 dB from CC7 32 to 127. Velocity matches on the same curve.

What remains is envelope shape on specific patches, measured with `--envelope`:

| Instrument | Agreement |
| --- | --- |
| organ | within 0.2 dB |
| piano | within 0.4 dB through the whole decay |
| flute | within 0.4 dB |
| strings | about 2 dB low for the first 140 ms, converging after |
| trumpet | 1.5 to 2.1 dB high across the sustain |

Modulation is not the cause. LFO depth and rate match exactly on every instrument tested, and the
trumpet's 0.7 dB 12 Hz wobble is in the sample rather than the synthesis - its articulation has
LFO to attenuation set to zero.

The concrete lead on trumpet: its articulation is attack 0 s, decay 12.18 s, **sustain 100 percent**.
A sustain of 100 percent means the decay segment has nothing to do, and ours is correctly flat. The
in-box synth falls about 2.5 dB after the attack and then levels off, so it appears to apply some
decay regardless. It is not simply ignoring a full sustain level, because the organ also has sustain
at 100 percent, a much shorter decay, and both engines are flat there.

Strings and trumpet deviate in opposite directions, so these are not one shared bug.

### Measuring level correctly

Use `--levels`, not `--compare`, for anything level related:

```powershell
.\out\x64\Release\synthspike-render.exe --levels capture.wav 11 2.0 0.3 0.6
```

It finds the first onset by energy and then steps by the known interval, which handles the unknown
lead in a capture carries plus the synth's own latency. Whole file `--compare` is misleading on a
sparse score: the velocity sweep is about half silence, and with the two files offset by 0.45 s a
whole file RMS ends up comparing notes against gaps.

Also check for a genuinely silent note before reporting a difference near the floor. Velocity 1
looked like an 8.9 dB discrepancy until both files turned out to contain exactly zero non-zero
samples there.

## Compatible and Modern modes

`SynthConfig::ForMode` selects between two profiles:

| | Compatible | Modern |
| --- | --- | --- |
| Render rate | 22050 Hz | whatever the sink asks for |
| Voices | 32 | 256 by default |
| Interpolation | linear | four point Hermite |
| Voice stealing | lowest channel number wins | quietest, releasing first |
| Limiter | off | on |

Compatible renders internally at 22050 Hz on purpose. The bandwidth limit is part of the sound
being reproduced, so rendering it at 48 kHz with better interpolation would produce something
cleaner than the thing it is supposed to match.

## Level, clipping and the limiter

The mix reaches full scale at about **20 simultaneous notes**, measured:

| Notes | 12 | 16 | 20 | 24 |
| --- | --- | --- | --- | --- |
| Peak | 0.698 | 0.871 | 1.000 | 1.000 |

That is not an accident of gain staging. `MasterGainDb` is set so the output level matches the
in-box synth, because a drop-in replacement that is louder or quieter changes how every existing
MIDI file sounds. Backing the gain off to buy headroom would break that.

So Modern mode limits instead. The limiter has instant attack and no lookahead, which means it
adds no latency and never overshoots, and it is transparent below threshold - the 12 and 16 note
peaks above are identical with it on. Compatible leaves it off: the in-box synth clips too, and is
documented as doing so, and its 32 voice cap bounds how far it can go.

The symptom to recognize is audible artifacts with **zero reported glitches**. A glitch counter
counts stream failures; clipping is a signal problem and will not move it.

## Ending a voice

A voice that is cut off mid waveform leaves a step in the signal and is heard as a click. This
happens more than it sounds like it would: the DLS default is that a second note on of the same
note kills the first, so any arpeggiator or trill triggers it constantly.

Voices therefore end with a short fade rather than an instant cut. A fade is used in preference to
waiting for a zero crossing because a zero crossing is unbounded in time - low frequency content
can be more than ten milliseconds from one - and the left and right channels of a panned voice do
not cross together. A fixed short fade is bounded, deterministic and inaudible.

`--clicks <wav>` reports the largest sample to sample step in a file, which is how this is checked.

How different the two actually are, measured on the demo score at a matched rate: the difference
signal sits **34 dB below** the program. That is subtle by design. The sound set is 22050 and
24000 Hz material, so it is already bandlimited to roughly 11 kHz and raising the render rate adds
no bandwidth that was not in the samples. **Modern mode is not worth more than Compatible because
it sounds cleaner** - it is worth more because of polyphony, timing, the variation banks, the nine
drum kits and MIDI 2.0 resolution.

## Building

```powershell
$msb = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
& $msb spikes\synthspike-dls\synthspike-dls.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64
```

Output lands in `out\x64\Release`. These projects are deliberately self-contained: they do not
need `SolutionDir` and they do not reference anything else in the repository.

## Running the DLS spike

```powershell
.\out\x64\Release\synthspike-dls.exe                      # defaults to the in-box gm.dls
.\out\x64\Release\synthspike-dls.exe --waves --instruments
.\out\x64\Release\synthspike-dls.exe path\to\other.dls --regions 12
```

## Running the render spike

```powershell
.\out\x64\Release\synthspike-render.exe --out demo.wav          # built-in demo score
.\out\x64\Release\synthspike-render.exe --compat --out old.wav
.\out\x64\Release\synthspike-render.exe --score my.txt --out my.wav
.\out\x64\Release\synthspike-render.exe --tuning 19             # pitch accuracy check
.\out\x64\Release\synthspike-render.exe --bench 256             # CPU cost at 256 voices
.\out\x64\Release\synthspike-render.exe --compare a.wav b.wav   # objective A/B
```

Score files are one event per line, times in seconds:

```
program <time> <channel> <bankMsb> <bankLsb> <program>
note    <time> <channel> <note> <velocity> <duration>
cc      <time> <channel> <controller> <value>
bend    <time> <channel> <value14>
```

### Verifying pitch

`--tuning` plays notes in isolation and measures the fundamental by normalized autocorrelation.
It reports the analyzer's own bias against a synthetic sine in a separate column, so estimator
error is never mistaken for engine error.

Expect around half a cent on sustained harmonic instruments. Struck and plucked sources read much
sharper at low notes because their partials really are inharmonic and autocorrelation is a poor
estimator for them; that is the material, not the engine. To tell the two apart, run the same test
at several rates with `--rate` - engine error would move, source inharmonicity does not.

### Comparing two renders

`--compare` reports RMS difference and where the difference sits by octave band. Listening tests
cannot settle whether Compatible matches the in-box synth, so this is how that gets judged.

Two things to know before trusting a number from it:

- **Same rate comparisons are exact.** Rendering the same configuration twice nulls to -200 dBFS,
  so the engine is fully deterministic and any residual is real.
- **Cross rate comparisons go through a linear resampler** inside the tool and are not trustworthy
  in the top octave. Match the rates with `--rate` when the number has to mean something.

A difference only a few dB below the program does not mean the two sound different. It usually
means they are time misaligned, because subtracting two similar signals that are offset gives back
roughly the sum of their energy. That is how the block quantized event dispatch bug was found.

## Capturing the in-box synth

```powershell
.\out\x64\Release\synthspike-capture.exe --silence 3 --out baseline.wav   # makes no sound
.\out\x64\Release\synthspike-capture.exe --calibrate                     # 2 s tone
.\out\x64\Release\synthspike-capture.exe --score compare-score.txt --out msgs.wav
```

The endpoint is opened shared mode, read only, using the mix format it already reports. Nothing
here can change an audio setting.

### Always calibrate before believing a level

**WASAPI loopback capture is taken after the endpoint volume control.** Measured on an endpoint
sitting at 53 percent: `--calibrate` emits a tone of known amplitude while capturing, and reported
a path gain of -9.63 dB, exactly the master volume, repeatable to 0.01 dB.

So a raw capture is attenuated by whatever the volume slider happens to be, and comparing two
captures taken at different volumes, or a capture against a file, is meaningless without
correcting for it. Getting this wrong inverts the answer: uncorrected, our render looked 5 dB
loud, when it was actually 4.7 dB quiet.

### Always take a silence baseline

Anything else playing lands in the capture. A baseline should read -200 dBFS. If it does not,
something is playing; `listsessions.cpp` names the process and shows its peak level. Capturing the
same score twice and nulling the two is the check that the capture was clean - two good captures
agree to about 99 dB below program.

## Playing it live

```powershell
.\out\x64\Release\synthspike-play.exe --list                    # MIDI inputs
.\out\x64\Release\synthspike-play.exe --in "Keystation"         # play from a keyboard
.\out\x64\Release\synthspike-play.exe --score demo.txt
.\out\x64\Release\synthspike-play.exe --compat --lowlatency
.\out\x64\Release\synthspike-play.exe --seconds 5               # unattended
```

With no MIDI source and no score the engine renders digital silence, which exercises the entire
audio path without making a sound. Use that to check a change before playing anything.

The render endpoint is opened shared mode with the mix format it already reports, so no device
setting is modified. In Modern mode the engine renders at the device rate and the audio engine does
no conversion; in Compatible mode it renders at 22050 Hz and the spike resamples.

MIDI arrives on a WinMM callback thread and crosses to the audio thread through a lock free single
producer single consumer queue, because the render thread must never block. Dropped messages are
counted rather than silently discarded.

### Latency is bounded by the device, not by us

`--lowlatency` asks `IAudioClient3` for the smallest shared mode period the device supports. Some
devices offer no headroom at all: on an RME HDSPe MADI FX the minimum period equals the default at
960 frames, so shared mode is fixed at 10 ms no matter what we request. The spike prints the
reported default and minimum so this is visible rather than assumed.

The sink writes **one period per callback**, not a full buffer. Topping the buffer up to full every
time would keep a whole buffer of audio queued ahead of the device, which the player feels as
latency for no benefit. On the endpoint above that was the difference between about 22 ms and about
2 ms of queued audio. `audio queued ahead` in the session summary is the number to watch; if it
approaches the buffer size, something has regressed.

Getting below the device period needs WASAPI exclusive or ASIO. Both take the endpoint away from
everything else on the machine, so neither can be the default for a synth that is meant to be
always available - a system synth holding an ASIO device would lock a DAW out of it.

### Telling a missing note from a late one

`notes with no voice` counts note ons that were received and understood but produced no sound,
because no region matched or the voice limit was reached. `MIDI dropped at queue` counts messages
that never reached the engine. Latency makes playing feel sluggish; it does not make notes vanish.
If either counter moves during a listening test, that is a defect rather than a latency complaint.

## UMP

`UmpDispatcher` turns Universal MIDI Packets into engine calls. It has no transport dependency on
purpose: a service transport receives raw UMP words through `IMidiBidirectional::SendMidiMessage`
and a client side host gets them from the SDK, and both can feed it directly.

It handles MIDI 1.0 and MIDI 2.0 channel voice messages, system reset, and enough system exclusive
to recognize GM System On and GS reset. Packets for other groups are ignored, unknown message types
are skipped by their declared size rather than desynchronizing the stream, and a partial packet at
the end of a buffer is left unconsumed for the caller to complete.

MIDI 2.0 resolution is carried through rather than reduced to seven bits. Channel controllers are
held normalized internally, and the 7 bit entry points map exactly as they did before, so the
MIDI 1.0 path is unchanged - verified by the level and spectral comparisons being identical
before and after that change.

```powershell
.\out\x64\Release\synthspike-render.exe --ump-test
```

Two things that test exists to catch, because both are silent failures:

- **A MIDI 2.0 note on with velocity zero is still a note on.** Only in MIDI 1.0 does velocity zero
  mean note off. A synthesizer that reuses its MIDI 1.0 path here drops notes.
- **A MIDI 2.0 program change carries its bank**, rather than depending on control change messages
  having arrived first.

Resolution is checked by sending two values that would collapse to the same seven bit number and
confirming the output level still differs: 0.269 dB apart for a 16 bit velocity, 0.135 dB for a
32 bit control change.

## Endpoint shape

One UMP Group, one bidirectional Function Block.

General MIDI is defined over sixteen channels - GM1 says "All 16 MIDI channels" - and sixteen
channels is exactly one UMP Group, so a second Group would have nothing to carry. The UMP
specification, section 6.2.1.1, says an input and output intended to work as a pair should be a
single Function Block spanning a single Group, and notes that MIDI-CI is more likely to operate
successfully that way. That is what MIDI-CI profiles and property exchange will need later.

## Identity Request

The in-box synth **cannot** answer one. It is an output only WinMM device with no MIDI input at
all, so it has no return path: on a machine with 57 MIDI inputs, none of them is the synth.
Answering is therefore new behavior rather than compatibility.

Ours answers, which is what the bidirectional Function Block is for. A manufacturer identifier is
one byte, or three bytes when the first is zero, and the reply length changes accordingly - thirteen
bytes or fifteen. Both shapes are covered by `--ump-test`.

It replies as Microsoft `00 00 41`, device family 11 (Windows 11), family model number 1 (this
synthesizer), software revision 1.0.0.0. Those constants are defined once in
`src/in-box/Inc/MidiDefs.h` and repeated in `SynthIdentity` only so the prototype keeps building
without the rest of the repository. Clearing the manufacturer identifier silences the reply rather
than sending zeros, because answering as somebody else is worse than not answering.

## Untrusted input

A user-supplied `.dls` is untrusted. The parser treats every length, count and offset in the file
as hostile: chunk sizes are validated against the enclosing range before the cursor moves, record
counts are checked against the bytes actually remaining before any allocation, and wave references
are resolved and validated at parse time so nothing downstream has to revalidate them.

The file is read into a heap buffer rather than memory mapped. A mapped view of a file that another
process truncates faults on access; a heap copy cannot be pulled out from under the parser.

`fuzz-dls.ps1` mutates the seed file and confirms every case either parses or is rejected cleanly:

```powershell
pwsh -File spikes\synthspike-dls\fuzz-dls.ps1 -Iterations 400
```

Run it against an AddressSanitizer build as well, which catches out-of-bounds reads that do not
happen to fault. Build with `/p:EnableASAN=true`, then put the `clang_rt.asan_dynamic-x86_64.dll`
matching your toolset version on `PATH` before running; a mismatched runtime fails with an
interception error rather than a useful result.

## Specifications

The DLS Level 1 and Level 2 specifications are MMA documents. They are **not** in this repository
and must not be added to it. `tools/specextract` converts a local copy to text for reference.

## Status

- [x] DLS Level 1 parser, validated against the in-box `gm.dls`
- [x] Voice engine and offline render, with tuning and CPU verification
- [x] Measured against the in-box synth for spectrum, level, velocity, CC7 and envelopes
- [x] WASAPI sink and live playback
- [x] UMP front end, MIDI 1.0 and MIDI 2.0 channel voice
- [ ] Live connection to Windows MIDI Services through the SDK
- [ ] Per-session render host and service transport
