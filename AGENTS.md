# Instructions for coding agents

Windows MIDI Services. See [README.md](README.md) for what the project is, and
[CONTRIBUTING.md](CONTRIBUTING.md) for contribution process.

## Writing

Everything in this repository is authored in **en-US**, including identifiers, comments, UI strings
and documentation. en-GB spellings are defects, not style preferences. Full rules and word list:
[.github/instructions/en-us-spelling.instructions.md](.github/instructions/en-us-spelling.instructions.md).
Check your work with `build/check_en_us_spelling.ps1 -Path <file or folder>`.

## Filing bug reports and designing test plans

Full guidance, including a write-up template, is in
[.github/skills/midi-bug-reports/SKILL.md](.github/skills/midi-bug-reports/SKILL.md). Read it before
drafting an issue or a test plan. The three rules that get broken most often:

1. **Scope the claim to what was actually tested.** One device or one driver failing is a
   device-scoped or driver-scoped defect until a differently-plumbed device also fails. Do not write
   *all*, *every*, *always* or *regression* unless the set was enumerated in the same report. Put the
   scope in the title.

2. **Test the native path before filing a WinMM bug.** WinMM goes through `wdmaud2.drv`; the
   Windows MIDI Services SDK does not. If the native UMP path also fails, the defect is in the
   service, transport or driver and should be filed as such. If only WinMM fails, it is in the WinMM
   client or the MIDI 1.0 translation. Report a result for each path, or mark it not tested.

3. **Report the API mode, and tell the user about Legacy API mode.** Record `midi api-mode get`. For
   a WinMM failure, also try Legacy mode (`UseLegacyMidi = 1`): if it fails there too, it is not a
   Windows MIDI Services regression. Offer Legacy mode to the user as a way to stay unblocked, and
   state what it costs — no multi-client, no built-in loopbacks, no Network MIDI 2.0 or Bluetooth LE
   MIDI, no USB MIDI 2.0 devices. See [docs/kb/how-to-change-api-mode.md](docs/kb/how-to-change-api-mode.md).

The issue form requires AI-generated analysis to be tagged **"AI Generated Content"**, and the
"personal observation" field must be the reporter's own words. Do not write that field for them.
