# Bug report write-up template

Fill every row. `Not tested` is an acceptable value; a missing row is not.

---

## Title

`[BUG]: <symptom> on <device or component> via <driver/transport> using <API path>`

## Scope of this report

| | |
|---|---|
| Devices tested | `<n>` of `<n>`: `<make model>` (`<driver + version>`, transport `<CODE>`), ... |
| Devices that reproduce | |
| Devices that do NOT reproduce | |
| Believed scope | device-specific / driver-specific / transport-wide / stack-wide — and why |

> Do not use *all*, *every*, *always*, *no*, *completely*, *regression* or *affects all users*
> unless the set is enumerated above.

## API path matrix

| Path | Tool used | Result |
|---|---|---|
| Native UMP (Windows MIDI Services) | `midi endpoint monitor` / MIDI Monitor | Pass / Fail / Not tested |
| WinRT MIDI 1.0 | | Pass / Fail / Not tested |
| WinMM (classic) | `midi1monitor.exe` | Pass / Fail / Not tested |

**What the matrix localizes:** `<service/transport/driver>` if the native path also fails, or
`<WinMM client / MIDI 1.0 translation>` if only WinMM fails.

## API mode

| | |
|---|---|
| Mode used for the repro (`midi api-mode get`) | Full (0) / Legacy (1) / Hybrid (2) |
| Result in Legacy API mode | Fails / Works / Not tested |
| Conclusion | Fails in Legacy too = not a Windows MIDI Services regression. Works in Legacy = defect is in the new stack. |

## Workaround guidance for the user

Legacy API mode (`midi api-mode set legacy`, elevated, then reboot) is a supported way to stay
unblocked while this is open. It gives up multi-client sharing, built-in loopbacks, Network MIDI 2.0,
Bluetooth LE MIDI, USB MIDI 2.0 device support, and new-style or custom MIDI 1.0 port names.
Return with `midi api-mode set full` and reboot.
See https://microsoft.github.io/MIDI/kb/how-to-change-api-mode/

## Steps to reproduce

1.
2.
3.

Expected:
Actual:

## Evidence

- `mididiag` output: (paste in full)
- Monitor output per path tested:
- Device instance ids:

## Observation (user writes this — do not fill it in for them)

## AI Generated Content

`<state which sections were produced by an agent>`
