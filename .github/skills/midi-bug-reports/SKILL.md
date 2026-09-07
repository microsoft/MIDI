---
name: midi-bug-reports
description: >
  Scope, reproduce and write up a Windows MIDI Services defect, or design a test plan for one.

  USE FOR: filing or drafting a GitHub issue against microsoft/MIDI; writing repro steps; deciding
  how broadly a failure applies; planning which API paths, devices, drivers and API modes to test;
  reviewing a bug report or test plan someone else wrote.

  DO NOT USE FOR: fixing the defect once it is scoped, general coding, KIR authoring, docs edits.
---

# Filing a Windows MIDI Services bug report

Three mistakes account for nearly every unusable report. Each has a hard rule below. Do not
skip a rule because the failure "obviously" applies everywhere — that judgment is exactly what
these rules exist to block.

## Rule 1: scope the claim to what you actually tested

One device or one driver failing is a **device-scoped or driver-scoped** defect until a second,
differently-plumbed device also fails. Windows MIDI Services spans several independent transports
and two USB drivers; a fault in one says nothing about the others.

- Write "2 of 2 devices tested" or "reproduced on the Foo X only; the Bar Y on the same driver is
  unaffected". Never write "all devices", "every vendor driver", "MIDI is broken", "no MIDI works".
- Test at least one **contrasting** device: a different driver (`usbmidi2.sys` vs `usbaudio.sys`
  vs a vendor driver) or a different transport (KS, KSA, LOOP, BLOOP, NET2UDP, BLEMIDI, DIAG, APP).
  If you cannot, say so explicitly: "no second device available; scope unverified".
- Put the scope in the title. `[BUG]: SysEx over 512 bytes truncated on Foo X under usbaudio.sys/KSA`
  is actionable. `[BUG]: SysEx is broken` is not.
- Words that require enumerated evidence in the same report: *all*, *every*, *always*, *no*,
  *completely*, *regression*, *affects all users*. If you did not enumerate the set, do not use them.
- A loopback or virtual endpoint failing does not implicate hardware transports, and the reverse.

## Rule 2: a WinMM report is incomplete until the native path is tested

The paths differ, so which one fails localizes the fault:

| Path | Route | Test with |
|---|---|---|
| Native UMP (Windows MIDI Services) | app -> SDK -> midisrv -> transport -> driver | `midi endpoint monitor`, MIDI Monitor (`midi2monitor`) |
| WinRT MIDI 1.0 | app -> WinRT MIDI 1.0 -> midisrv -> transport -> driver | a WinRT MIDI 1.0 app |
| WinMM (classic) | app -> `wdmaud2.drv` -> midisrv -> transport -> driver | `midi1monitor.exe`, `midi1enum.exe` |

- **Native UMP also fails** -> the defect is in the service, transport or driver. File it there and
  note WinMM as an affected consumer. Do not title it as a WinMM bug.
- **Native UMP succeeds, WinMM fails** -> the defect is in the WinMM client or the MIDI 1.0
  translation (port mapping, port naming, message translation, buffer sizes). Different component,
  different fix. Say so.
- The reverse also applies: when reporting a native-path defect, state whether WinMM sees it.
- Report the result for every cell you tested and mark the rest **not tested**. An empty cell is
  information; a missing table is not.

## Rule 3: report the API mode, and tell the user about Legacy API mode

`UseLegacyMidi` in `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32` selects
0 = Full Windows MIDI Services (default), 1 = Legacy, 2 = Hybrid. See
[docs/kb/how-to-change-api-mode.md](docs/kb/how-to-change-api-mode.md).

- Record the mode the repro ran under: `midi api-mode get`.
- For any WinMM-path failure, also try Legacy mode. It removes midisrv and `wdmaud2.drv` from the
  path entirely, so the result is diagnostic:
  - **Fails in Legacy too** -> not a Windows MIDI Services regression. Look at the device, the
    vendor driver, or the application.
  - **Works in Legacy** -> the defect is in the new stack, and the report should say which
    component the Rule 2 matrix pointed at.
- Tell the user in the report that Legacy mode is a supported way to get unblocked while the bug is
  open, and state the cost so the choice is informed: no multi-client sharing, no built-in loopbacks,
  no Network MIDI 2.0 or Bluetooth LE MIDI, no USB MIDI 2.0 devices in MIDI 2.0 mode, and no new-style
  or custom MIDI 1.0 port names.
- Switching: `midi api-mode set legacy` from an **elevated** prompt, then reboot. Return with
  `midi api-mode set full` and reboot. Do not run MIDI Console, MIDI Settings or `midicheckservice`
  while in Legacy mode — they try to start the service.

## Evidence every report needs

- Full `mididiag` output (MIDI Settings > Troubleshooting), which carries the Windows build and stack version.
- Device make, model, connection, and the driver in use with its version.
- The transport code from the endpoint's device instance id (`MIDIU_<CODE>_...`).
- Monitor output from each path tested, not a paraphrase of it.
- A repro using a simple tool (`midi.exe`, `midi1monitor.exe`, MIDI Monitor, Pocket MIDI) rather than
  only a DAW. DAWs have their own defects.

## Test plans

A test plan for client-visible behavior covers the same three axes: API path (native UMP / WinRT MIDI 1.0
/ WinMM), device and driver (at least two, differently plumbed), and API mode where the behavior could
plausibly differ. State the axes you deliberately excluded and why.

## Repository rules that apply to the write-up

- The issue form requires AI-generated analysis to be tagged **"AI Generated Content"**. Tag it.
- The "personal observation that led to the bug report" field must be the **user's own words**. Do not
  draft it for them; ask them for it.
- Search existing issues first; duplicates are closed.
- en-US spelling, per [.github/instructions/en-us-spelling.instructions.md](.github/instructions/en-us-spelling.instructions.md).

Use [references/report-template.md](references/report-template.md) for the write-up.
