# midicrashhandler

The Windows Error Reporting (WER) runtime exception module for the Windows MIDI Services tools. When one of the tools crashes, it opens MIDI Troubleshooting and Repair on its Crash dumps page, which says which app stopped, shows the crash dump once Windows has saved it, and offers to save a MIDI service dump to go with it.

## How the pieces fit

| Piece | What it does |
| --- | --- |
| `midicrashhandler.dll` (this project) | WER loads it into WerFault.exe, outside the crashed app, for every kind of crash, including the fail fast that WIL, C++/WinRT and WinUI end in. It starts `miditroubleshooter.exe` from its own folder with `--crashed-pid`, `--crashed-exe` and `--werfault-pid`, and never claims the crash, so WER goes on as it would without it. |
| `midi-app-shared\CrashHandlerRegistration.h` | `midiapp::RegisterCrashHandler()`, the first call in each GUI tool's `wWinMain`. It calls `WerRegisterRuntimeExceptionModule` with this DLL's path. MIDI Troubleshooting and Repair doesn't call it, and the console tools don't either: a console tool opening a window when it crashes in a script would be a surprise. |
| The Tools installer | `build\build-all.ps1` generates `_CrashDumps.wxs`. It lists this DLL under `HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\RuntimeExceptionHelperModules`, which WER requires before it loads any module, and adds a `LocalDumps` entry for every tool, GUI and console. |
| MIDI Troubleshooting and Repair | The Crash dumps page (`CrashDumpsPanel`, `CrashDumps`). If it's already open, the new copy passes the crash to it and exits. |

`miditroubleshooter.vcxproj` builds this project first and copies the DLL into its own output folder, so the DLL always sits next to the app it starts, in a development build as well as when installed.

## Where the dumps go

WER writes each tool's dumps to `%LOCALAPPDATA%\Microsoft\MIDI\CrashDumps\<exe name>`, keeping the newest 10. The folder is per user so the customer can delete the dumps and WER can replace the oldest, and per app so one app that keeps crashing can't push out the others' dumps. A folder under `%ProgramData%` doesn't work: normal users can't delete files there, so WER can't replace the oldest dump and stops saving new ones. The dump type is a custom dump with the heap, thread stacks and handles but not the code of every loaded DLL.

MIDI service dumps go in the `midisrv` folder beside them, named after the crash they go with, and the page keeps the newest 10.

## Things to know

- The callback runs about 135 ms after the crash, before the dump exists. WerFault.exe keeps the dump open until it exits, which is also when the crashed app exits, so the Troubleshooter waits for WerFault.exe before it says the dump is saved.
- With the DLL registered by the app but not listed in HKLM, WER never loads it, and still writes the dump.
- WerFault.exe runs as the user in their session, so the Troubleshooter opens as the user, without administrator rights. It doesn't ask for them when it opens for a crash. Only **Save a MIDI service dump** does.
- The handler skips crashes in `miditroubleshooter.exe` itself, so a Troubleshooter that crashes as it opens can't keep opening itself.
- Not tested yet: 32-bit apps (none of the tools are), packaged apps, and tools running elevated.

The measurements behind this design are in `src\prototypes\wer-crash-helper\README.md`.

## Trying a development build

A development build needs the same two registry entries the installer writes. Run these from an elevated PowerShell, as your own account so `%LOCALAPPDATA%` means your folder, with `<out>` being `src\in-box\vsfiles-sdk\out\miditroubleshooter\x64\Release`:

```powershell
$wer = 'HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting'
New-Item "$wer\RuntimeExceptionHelperModules" -Force | Out-Null
New-ItemProperty "$wer\RuntimeExceptionHelperModules" -Name '<out>\midicrashhandler.dll' -PropertyType DWord -Value 0 -Force | Out-Null

# one of these for each tool you want to crash
$key = New-Item "$wer\LocalDumps\midiclock.exe" -Force
New-ItemProperty $key.PSPath -Name DumpFolder -PropertyType ExpandString -Value '%LOCALAPPDATA%\Microsoft\MIDI\CrashDumps\midiclock' -Force | Out-Null
New-ItemProperty $key.PSPath -Name DumpType -PropertyType DWord -Value 0 -Force | Out-Null
New-ItemProperty $key.PSPath -Name CustomDumpFlags -PropertyType DWord -Value 0x1B25 -Force | Out-Null
New-ItemProperty $key.PSPath -Name DumpCount -PropertyType DWord -Value 10 -Force | Out-Null
```

Remove both when you're done. The tools find the DLL in the Troubleshooter's output folder on their own.
