---
description: "Use when writing or editing any text in this repository: UI strings, .resw and .rc resources, code comments, identifiers and variable names, documentation, commit messages and PR descriptions. Windows MIDI Services ships in en-US, so en-GB spellings such as colour, centre, cancelled, travelling, initialise and behaviour are defects."
applyTo: "src/**, docs/**, samples/**, build/**, *.md"
---

<!-- en-us-spelling-check: ignore-file (the table below quotes the spellings it forbids) -->

# en-US spelling is required

Everything authored in this repository is en-US. That covers all four of these, not just the
first:

1. **User-visible strings** — `.resw` values, `.rc` string tables, console output, log messages
2. **Identifiers** — variable, function, resource key and `#define` names (`SendCanceled`, not
   `SendCancelled`)
3. **Comments** — including one-line comments and XML doc comments
4. **Documentation** — everything under `docs/`, plus READMEs and commit messages

## The ones that actually get through

| Do not write | Write |
|---|---|
| cancelled, cancelling | canceled, canceling |
| travelling, traveller | traveling, traveler |
| labelling, modelling, signalling, marshalling | labeling, modeling, signaling, marshaling |
| colour, behaviour, honour, favour | color, behavior, honor, favor |
| centre, centred, metre | center, centered, meter |
| initialise, optimise, serialise, recognise, organise | initialize, optimize, serialize, recognize, organize |
| initialisation, packetisation, synchronisation | initialization, packetization, synchronization |
| analyse, catalogue, analogue | analyze, catalog, analog |
| grey, programme, artefact, licence, defence | gray, program, artifact, license, defense |
| judgement, acknowledgement, enquiry | judgment, acknowledgment, inquiry |
| whilst, amongst, learnt, spelt | while, among, learned, spelled |

Words that look British but are correct en-US, so do not "fix" them: **advertise, otherwise,
surprise, exercise, compromise, enterprise, expertise, cancellation** (two l's in both),
**analyses** (the noun), **discarded**.

`dialogue` is correct en-US for a conversation, but a UI dialog is a **dialog**.

## Verify before handing work back

Do not rely on reading it back. Run the check:

```powershell
pwsh -File build\check_en_us_spelling.ps1                                    # whole repo
pwsh -File build\check_en_us_spelling.ps1 -Path src\api\Client\WinRT\user-tools
```

It prints `file / line / found -> suggestion` and exits non-zero if anything is found, so it can
gate a build. It understands `SNAKE_CASE` and `camelCase`, which a plain word-boundary search
misses.

If you add a term to it, only add spellings that are **always** wrong in en-US, so a hit is never
a judgment call.

## Renaming an identifier

Change the name **and** every reference, including the `.resw` or `.rc` key and the
`GetString(L"...")` lookup that pairs with it. A resource key renamed on only one side compiles
and then fails at runtime with a missing string.
