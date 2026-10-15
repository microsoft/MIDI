# Windows MIDI Sound Bank Synth

Windows MIDI Sound Bank Synth turns SoundFont 2 banks (`.sf2` files) into synths that any MIDI app on the PC can play. Each synth you add gets its own MIDI endpoint, so a DAW, a notation app or the MIDI console sees it like any other device. You can run several synths at once, and they all play through one audio device.

It's one of the in-box MIDI tools, and it builds with the rest of them. It isn't in the Tools installer yet, though. Each synth is a virtual device, so the app has to wait until the Virtual Device transport works in the released service.

## What it does

- **Loads a bank from anywhere on the PC.** Pick an `.sf2` file and the app makes a synth from it. Every file is treated as hostile: the loader checks every size, count and index before it uses it, and a damaged file shows an error on its card instead of taking the app down.
- **Publishes one MIDI endpoint per synth.** Each synth is a virtual device created through the Windows MIDI Services SDK. It listens on group 1, all 16 channels. The endpoint keeps the same id across restarts, so apps find it again.
- **Plays MIDI 1.0 and MIDI 2.0 messages.** MIDI 2.0 adds 16-bit velocity, 32-bit controllers, bank and program in one message, registered controllers, per-note pitch bend, per-note controllers, per-note management and the pitch 7.9 note attribute.
- **Answers the same MIDI-CI as the in-box General MIDI synth.** That means Discovery, the endpoint inquiry, Process Inquiry, and Property Exchange with `ResourceList`, `DeviceInfo`, `ChannelList` (with subscriptions), `ProgramList` and `ChCtrlList`. `ProgramList` lists the bank's own presets, split into melodic programs and drum kits. `ChCtrlList` lists the 17 controllers every channel responds to and the value each one resets to.
- **Tells an app how each channel is set.** Process Inquiry's MIDI Message Report sends back a channel's pitch bend, controllers, bend range, tuning, program and bank, and channel pressure. An app can ask for everything or only what differs from the reset values. A value that arrived as a 32-bit MIDI 2.0 message comes back exactly as it was sent.
- **Plays through WASAPI, shared or exclusive.** Shared mode uses the smallest period the device allows. Exclusive mode lets you pick a 3, 5, 10, 20 or 40 ms buffer. The app lets go of the audio device 5 seconds after the last note stops, so other apps can use it, and opens it again when a note arrives.
- **Behaves like the other MIDI tools.** It has the same title bar, appearance settings and always-on-top button. It can start with Windows, start minimized, and keep running in the notification area when you close the window.

## What it doesn't do yet

- No ASIO output.
- No SoundFont 3 banks (compressed samples). The card says so when you pick one.
- No DLS banks.
- No translated strings yet. Every string is in `Strings/en-US/Resources.resw`, including the names the synths send over MIDI-CI, so adding a language is a matter of adding a folder.
- The icon is a placeholder.

## Folders

| Folder | What's in it |
| --- | --- |
| `src/in-box/user-tools/midi-sound-bank-synth` | The WinUI 3 app. It uses the shared files in `src/in-box/user-tools/midi-app-shared`. |
| `src/in-box/user-tools/midi-sound-bank-synth/engine` | The synth engine as a static library: the SoundFont parser, the voice engine, MIDI-CI and Property Exchange, and the WASAPI output. It has no UI code, so the tests can use it directly. It reuses the in-box synth's reverb and chorus from `src/in-box/Libs/MidiSynthLib`. |
| `src/in-box/Test/Tools/Midi2.MidiSoundBankSynth.unittests` | TAEF tests for the parser (including a test that damages thousands of banks at random), the voice engine and MIDI-CI. They build their own small banks in memory, so no `.sf2` file is needed. |

Synths are saved to `%LOCALAPPDATA%\Microsoft\MIDI Sound Bank Synth\synths.json`. The app reads that file as untrusted input too. App settings are under `HKEY_CURRENT_USER\Software\Microsoft\Windows MIDI Services\Tools\midisoundbanksynth`.

## Building

Open `src/in-box/Midi2-AppSDK.sln`, pick `x64` or `ARM64`, and build the `midisoundbanksynth` project. That also builds the engine and the Windows MIDI Services SDK. The app lands in `src/in-box/vsfiles-sdk/out/midisoundbanksynth/<platform>/<configuration>`. It runs unpackaged, so the Windows App SDK runtime and the Windows MIDI Services SDK runtime have to be installed.

`build\build-all.ps1` builds it along with everything else in that solution, but doesn't add it to the Tools installer.

To run the tests, build `Midi2.MidiSoundBankSynth.unittests` and run it with TAEF:

```powershell
te.exe src\in-box\vsfiles-sdk\out\tests\x64\Release\Midi2.MidiSoundBankSynth.unittests.dll
```

## Why "Sound Bank"

SoundFont is Creative's trademark, so the app doesn't use it as a product name. It still says "SoundFont 2" where it means the file format. Names inside the code that describe the format, like the engine's `SoundFontSynth` namespace and `SoundFontSynthEngine.vcxproj`, stay as they are.

## Replacing the icon

The window and taskbar icon is `Assets/AppIcon.ico`. To replace it, make a new one from a square PNG with `build\make_app_icon.ps1`.

## Trademarks

SoundFont is a registered trademark of Creative Technology Ltd.
