# WER crash helper prototype

**PROTOTYPE.** Not part of any solution, installer or build pipeline.

This checks whether Windows Error Reporting (WER) can start our own helper app when a Windows MIDI Services app crashes, so the helper can show where the crash dump went and offer to capture a midisrv dump. It answers three questions before anyone builds the real feature:

1. Does a WER runtime exception module get called for an access violation, an unhandled C++ exception, and fail-fast crashes (`__fastfail`, WIL `FAIL_FAST`, and the `RoFailFastWithErrorContext` that a WinUI 3 / C++/WinRT app ends in)?
2. Can that module, running inside WerFault.exe, start a separate process without holding WER up, and does that process keep running after WerFault.exe exits?
3. Does WER LocalDumps still write `<exe>.<pid>.dmp` with the module registered, and can the helper tell when the file is complete?

It also records which of those crashes an in-process `SetUnhandledExceptionFilter` sees, for comparison.

## What's here

| Path | What it is |
| --- | --- |
| `module\` | `WerHelperModule.dll`, the WER runtime exception module. Its callback logs what it saw, starts `WerCrashHelper.exe`, and never claims the crash, so WER carries on as normal. |
| `helper\` | `WerCrashHelper.exe`, a stand-in for the future helper UI. It has no window. It logs when it ran, when the crashed app and WerFault.exe exit, and when the dump file is complete. |
| `crashtest\` | `WerCrashTest.exe <kind>`, which registers the module, sets an unhandled exception filter, and crashes the way you ask. Run it with no arguments to see the kinds. |
| `build.ps1` | Builds all three into `out\<platform>\<configuration>\`. |
| `setup-registry.ps1` | Elevated, from your own account. Lists the module under `RuntimeExceptionHelperModules` (HKLM, or HKCU with `-Scope HKCU`) and turns on LocalDumps for `WerCrashTest.exe` only: full dumps, at most 10, to `%LOCALAPPDATA%\Microsoft\MIDI\CrashDumps\WerCrashTest`. That's one folder per user and per app. |
| `remove-registry.ps1` | Elevated. Removes everything `setup-registry.ps1` added. Add `-RemoveDumps` to delete the test dumps too. |
| `run-tests.ps1` | Not elevated. Crashes the test app once per kind and prints a table built from the logs. Results are appended to `out\results.csv`. |

Logs from all three binaries go to `out\<platform>\<configuration>\logs\`, one file per crashed process ID. Every line starts with a precise timestamp, so the app, WerFault.exe and the helper share one timeline.

## Running it

```powershell
.\build.ps1
# elevated:
.\setup-registry.ps1
# not elevated:
.\run-tests.ps1
# elevated, when done:
.\remove-registry.ps1 -RemoveDumps
```

## Results (October 21 2026, Windows build 29683, x64, full dumps of about 17-20 MB)

Times are from the start of the WER callback. Every row was run with the module listed in HKLM.

| Crash | Exit code | Unhandled exception filter | WER callback | Callback took | Helper running | Dump appears | Dump complete | WerFault exits | Helper alive after |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Access violation | `0xC0000005` | yes | yes | 8.5 ms | 15.6 ms | 1735 ms | 2044 ms | 1988 ms | yes |
| Unhandled C++ exception | `0xE06D7363` | yes | yes | 4.7 ms | 9.8 ms | 1227 ms | 1485 ms | 1485 ms | yes |
| `std::terminate` | `0xC0000409` | no | yes | 5.0 ms | 10.2 ms | 1730 ms | 2040 ms | 2040 ms | yes |
| `__fastfail` | `0xC0000409` | no | yes | 5.0 ms | 10.4 ms | 1222 ms | 1532 ms | 1476 ms | yes |
| WIL `FAIL_FAST_HR` after an originated error (`RoFailFastWithErrorContext`) | `0xC000027B` | no | yes | 4.7 ms | 9.8 ms | 1277 ms | 1586 ms | 1531 ms | yes |
| WIL `FAIL_FAST_HR`, nothing originated | `0xC0000409` | no | yes | 5.5 ms | 10.7 ms | 1276 ms | 1535 ms | 1480 ms | yes |
| C++/WinRT error to `RoFailFastWithErrorContext` (WinUI 3 style) | `0xC000027B` | no | yes | 5.2 ms | 10.5 ms | 1123 ms | 1433 ms | 1376 ms | yes |

Other things the runs showed:

- The callback runs about 135 ms after the crash, before WER starts the dump. The dump is written by WerFault.exe and stays locked (sharing violation) until WerFault.exe exits, which is also when the crashed process exits.
- WerFault.exe was not in a job, ran in the user's session as the user, and `CREATE_BREAKAWAY_FROM_JOB` succeeded. The helper kept running after WerFault.exe exited.
- The module can read the context block the app registered, out of the crashed process, with `ReadProcessMemory`.
- With the module listed but the app not registering it, WER never loads the DLL, and the dump is still written.
- Letting WER show UI (no `WerSetFlags(WER_FAULT_REPORTING_NO_UI)`) gave the same results.
- **The dump folder must let the crashing user delete files.** WerFault.exe writes the dump as the crashing user, and with `DumpCount` dumps already in the folder it deletes the oldest before it writes the new one. The first round used `%ProgramData%\Microsoft\MIDI\CrashDumps`, which inherits Read and Write for Authenticated Users but no Delete. There, the 11th crash got no dump at all, and the old dumps stayed. A normal user couldn't remove them either.
- **With a per-user, per-app folder, rotation works.** The retest used `%LOCALAPPDATA%\Microsoft\MIDI\CrashDumps\WerCrashTest` with `DumpCount` 10. WER created the folders itself, owned by the user. Over 12 crashes, every crash wrote its dump. Crashes 11 and 12 each deleted the oldest dump, and the folder kept exactly 10. The Application event log had only the expected crash records, with no WER errors. The only change between the two rounds was the folder, so the `%ProgramData%` permissions caused the missing dumps.
- A per-user folder also keeps one user's dumps away from other users. In the `%ProgramData%` folder, Everyone could read them.

Not tested: listing the module in HKCU only, 32-bit apps (which need the WOW6432Node key and a 32-bit module), packaged apps, and apps running elevated.

## How the real feature would differ

The real helper would be the MIDI Troubleshooting app, opened on a crash dumps page with the crashed app's process ID and exe name. Its midisrv button would use the existing elevated service dump code. The installer would write the LocalDumps and `RuntimeExceptionHelperModules` entries for each MIDI app. None of that is built here.

That's now built: see `src\in-box\user-tools\midi-crash-handler\README.md`. The real dumps are custom dumps (heap, stacks and handles) rather than the full dumps used here.
