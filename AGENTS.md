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

## The configuration file is off limits

Windows MIDI Services keeps its configuration in `%ALLUSERSPROFILE%\Microsoft\MIDI` in a
`.midiconfig.json` file named by a registry value. **The JSON schema, the file name, the folder and
the registry value are implementation details, not a contract.** They can change in any release,
without notice.

There are exactly two supported ways to read or change that configuration:

- **The Windows MIDI Services API** — the `Windows.Devices.Midi2.ServiceConfig` namespace and the
  per-transport config types. It serializes access with the service, merges entries instead of
  rewriting the file, makes its own backups, validates what it writes, and applies the change to the
  running service as well as to disk.
- **The in-box tools built in this repository** — the MIDI Settings app, the `midi` console and the
  PowerShell module — which call that same API.

Everything else is unsupported:

- **Do not write, ship, publish or recommend a script or tool that opens, parses, edits, merges,
  backs up or restores the configuration file directly.** Care taken does not make it supported.
  Hash checks, exclusive file handles, before-images and temp-file swaps still race the service and
  the Settings app, still leave debris in `ProgramData`, and still break the day the format changes.
- **No one outside Microsoft may ship a product that manipulates the configuration file directly**,
  for any reason, including backup, restore, migration or "personalization recovery".
- If the API cannot express what is needed, that is an API gap. Say so, and file an issue for it.
  **Do not fall back to the file.** An unfinished feature is a better outcome than a tool that
  corrupts a customer's MIDI configuration after the next update.

### Developer-only tools inside this repository

A tool in this repository that must touch the file directly — a test fixture, a diagnostic, a repair
tool — is allowed, but it has to carry this disclaimer **in the source and in any documentation for
it**, so that neither a human nor another agent learns the pattern from it:

> **UNSUPPORTED — DO NOT COPY THIS APPROACH.** This code reads and writes the Windows MIDI Services
> configuration file directly. Microsoft does not support direct manipulation of that file by
> anything other than the in-box MIDI tools. Its format, its name, its location and the registry
> value that selects it are implementation details and can change in any release without notice, so
> this code can corrupt a configuration or stop working at any time. It exists for development and
> diagnostics inside the Windows MIDI Services repository only. Applications and third-party tools
> must use the Windows MIDI Services API (`Windows.Devices.Midi2.ServiceConfig`) instead.

## Writing

Everything in this repository is authored in **en-US**, including identifiers, comments, UI strings
and documentation. en-GB spellings are defects, not style preferences. Full rules and word list:
[.github/instructions/en-us-spelling.instructions.md](.github/instructions/en-us-spelling.instructions.md).
Check your work with `build/check_en_us_spelling.ps1 -Path <file or folder>`.

**Never hard-wrap markdown.** A paragraph is one line, however long. So is a bullet, including its continuation text, and so is a table row. Put newlines only between blocks — between paragraphs, between bullets, and around headings and code fences. Code fences and front matter keep their own line breaks. There is no column limit. This applies to every markdown file here, to GitHub issues and pull request descriptions, and to reports filed against other repositories.

**Public documentation is written for an eighth grader.** Short sentences, everyday words, and the plain term rather than the clever one. Contractions are welcome. Explain why a setting matters, not just what it is.

**Leave the jargon out.** That includes the figures of speech agents reach for by reflex: *load-bearing*, *smoking gun*, *deep dive*, *surface it*, *unpack*, *low-hanging fruit*, *rabbit hole*, *move the needle*. Write what actually happens instead — "removing this breaks every published link", "this is the proof", "show it in the UI".

**Bug reports get the same rules, plus one more: be brief.** Technical language is fine in a bug report. Length is not. Include the detail a developer needs and stop. Do not bury a fact in a paragraph of setup, restate the problem three ways, or summarize what you just wrote. Jargon and length are the two things that make an agent-written bug report useless.

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
