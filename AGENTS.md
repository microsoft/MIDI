# Instructions for coding agents

Windows MIDI Services. See [README.md](README.md) for what the project is, and
[CONTRIBUTING.md](CONTRIBUTING.md) for contribution process.

## This repository is public

You may be given access to non-public source or documents to investigate a problem. Treat them as
input to your decisions only. **Nothing from them may appear in this repository, or in issues,
pull requests or commit messages** — not file or symbol names, not internal interfaces or keys, and
not the mechanism behind a behavior. Describing externally observable behavior is fine; explaining
how it is implemented internally is not. If a document seems to need the internal mechanism to make
sense, rewrite it in terms of what a customer can observe.

## Writing

Everything in this repository is authored in **en-US**, including identifiers, comments, UI strings
and documentation. en-GB spellings are defects, not style preferences. Full rules and word list:
[.github/instructions/en-us-spelling.instructions.md](.github/instructions/en-us-spelling.instructions.md).
Check your work with `build/check_en_us_spelling.ps1 -Path <file or folder>`.

## Writing or reviewing code

Read [.github/skills/midi-contributing/SKILL.md](.github/skills/midi-contributing/SKILL.md) before
you edit any source file. The rules that get broken most often:

1. **Changes to code that ships in Windows must be wrapped in a servicing gate (KIR)**, with the
   original behavior preserved exactly when the gate is disabled. The trigger is where the file
   lives, not how small the diff is. Where the branch goes — call site, a net-new function, in
   place, or a net-new class chosen at the factory — depends on the shape of the change.
2. **SAL-annotate every function parameter**, and use `_Use_decl_annotations_` with bare parameters
   in the `.cpp` when the declaration is in a header.
3. **User-facing strings go in resource files** (`.resw` or `.rc`), never inline in code.
4. **Review your own diff for use after free, TOCTOU, escaping exceptions, lock inversions and
   untrusted input** before submitting. An exception escaping a thread body or an `HRESULT`
   function in `midisrv` takes MIDI down for the whole machine.
5. **Do not change public WinRT API surface without asking.** It ships in box and applications
   depend on it.

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
