# MIDI 2.0 SoundFont Synth

MIDI 2.0 SoundFont Synth turns SoundFont 2 banks (`.sf2` files) into synths that any MIDI app on the PC can play. Each synth you add gets its own MIDI endpoint, so a DAW, a notation app or the MIDI console sees it like any other device. You can run several synths at once, and they all play through one audio device.

The app runs on its own. It isn't part of the MIDI Settings app, and it doesn't ship in Windows. It has its own solution, build script and MSIX package.

## What it does

- **Loads a bank from anywhere on the PC.** Pick an `.sf2` file and the app makes a synth from it. Every file is treated as hostile: the loader checks every size, count and index before it uses it, and a damaged file shows an error on its card instead of taking the app down.
- **Publishes one MIDI endpoint per synth.** Each synth is a virtual device created through the Windows MIDI Services SDK. It listens on group 1, all 16 channels. The endpoint keeps the same id across restarts, so apps find it again.
- **Plays MIDI 1.0 and MIDI 2.0 messages.** MIDI 2.0 adds 16-bit velocity, 32-bit controllers, bank and program in one message, registered controllers, per-note pitch bend, per-note controllers, per-note management and the pitch 7.9 note attribute.
- **Answers the same MIDI-CI as the in-box General MIDI synth.** That means Discovery, the endpoint inquiry, and Property Exchange with `ResourceList`, `DeviceInfo`, `ChannelList` (with subscriptions) and `ProgramList`. `ProgramList` lists the bank's own presets, split into melodic programs and drum kits.
- **Plays through WASAPI, shared or exclusive.** Shared mode uses the smallest period the device allows. Exclusive mode lets you pick a 3, 5, 10, 20 or 40 ms buffer. The app lets go of the audio device 5 seconds after the last note stops, so other apps can use it, and opens it again when a note arrives.
- **Behaves like the other MIDI tools.** It has the same title bar, appearance settings and always-on-top button. It can start with Windows, start minimized, and keep running in the notification area when you close the window.

## What it doesn't do yet

- No ASIO output.
- No SoundFont 3 banks (compressed samples). The card says so when you pick one.
- No DLS banks.
- No localized strings yet. Every string is in `app/Strings/en-US/Resources.resw`, so adding a language is a matter of adding a folder. The display name in the package manifest is plain text for now.
- The icon is a placeholder.

## Folders

| Folder | What's in it |
| --- | --- |
| `engine` | The synth engine as a static library: the SoundFont parser, the voice engine, MIDI-CI and Property Exchange, and the WASAPI output. It has no UI code, so the tests can use it directly. It reuses the in-box synth's reverb and chorus from `src/in-box/Libs/MidiSynthLib`. |
| `tests` | TAEF tests for the parser (including a test that damages thousands of banks at random), the voice engine and MIDI-CI. They build their own small banks in memory, so no `.sf2` file is needed. |
| `app` | The WinUI 3 app. It uses the shared files in `src/in-box/user-tools/midi-app-shared`. |
| `packaging` | The MSIX manifest template and the source image for the package logos. |

Synths are saved to `%LOCALAPPDATA%\MIDI 2.0 SoundFont Synth\synths.json`. The app reads that file as untrusted input too. App settings are under `HKEY_CURRENT_USER\Software\Microsoft\Windows MIDI Services\Tools\midisoundfontsynth`. A packaged copy of the app keeps both in its own package data instead.

## Building

You need Visual Studio with the C++ desktop and WinUI workloads, the Windows SDK, and TAEF (it comes with the Windows Driver Kit). The Windows MIDI Services SDK comes from NuGet, so you don't need the rest of this repository built first.

To build and run the app without packaging it, open `MidiSoundFontSynth.sln`, pick `x64` or `ARM64`, and build. The app is in `VSFiles\<platform>\<configuration>\midisoundfontsynth`. It runs unpackaged, but the Windows App SDK 1.8 runtime has to be installed.

To make the packages, run this from the repository root:

```powershell
.\build\build-soundfont-synth.ps1
```

That builds the solution for x64 and Arm64, runs the engine tests, and writes an `.msix` for each platform plus an `.msixbundle` to `src\standalone-apps\soundfont-synth\VSFiles\package`. Useful parameters:

| Parameter | What it does |
| --- | --- |
| `-Version` | The package version, like `0.1.0.0`. The last number must be 0 for the Store. |
| `-IdentityName`, `-Publisher`, `-PublisherDisplayName` | The package identity. For the Store, copy these from the Product identity page in Partner Center. |
| `-Platform` | Build just `x64` or just `Arm64`. |
| `-SkipTests` | Skip the engine tests. |
| `-Sign` | Sign the packages with `build\sign-files.ps1`. Not needed for a Store upload, because the Store signs the package. |
| `-Register` | Register the x64 package layout for the current user so you can run it packaged. This needs Developer Mode. |

To remove a package you registered for testing, run `Get-AppxPackage MIDI2SoundFontSynth | Remove-AppxPackage`. If you used a different `-IdentityName`, use that name instead.

## Replacing the icon

The package logos are drawn from `packaging/AppIcon-source.png` every time the build script runs, so replace that file with a square PNG at least 1024 pixels wide. The window and taskbar icon is `app/Assets/AppIcon.ico`. You can make it from the same PNG with `build\make_app_icon.ps1`.

## Trademarks

SoundFont is a registered trademark of Creative Technology Ltd.
