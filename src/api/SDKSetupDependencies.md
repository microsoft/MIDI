# SDK Setup Dependencies

The SDK relies on the following to be present on the target system.

## Registry Entries

`Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows MIDI Services\Desktop App SDK Runtime`

| Value | Type | Description |
| --- | --- | --- |
| `CollectMidiLogs` | REG_SZ | Full path and file name for CollectMidiLogs.ps1 |
| `MidiFixReg` | REG_SZ | Full path and file name for midifixreg.exe |
| `MidiDiag` | REG_SZ | Full path and file name for mididiag.exe |

## Service Registry Entries


## SDK Files

| File Name | Description |
| --- | --- |
| `Windows.Devices.Midi2.dll` | WinRT SDK implementation |


## Additional Files

There are several utilities that first and third-party customer support expects to have available on a PC with Windows MIDI Services installed. Several of these are also just useful to end users for general use.

These files all need to be in a location which is part of the system path for all users.

| File Name | Description |
| --- | --- |
| `CollectMidiLogs.ps1` | PowerShell script to collect MIDI logs |
| `CollectMidiLogs.cmd` | Console launcher for the PowerShell script |
| `midifixreg.exe` | Executable to fix MIDI registry entries |
| `mididiag.exe` | Primary support executable which dumps the state of Windows MIDI Services. Also provides CFR status. |
| `midi1enum.exe` | Executable to enumerate MIDI devices using WinMM |
| `midi1monitor.exe` | Basic MIDI 1 monitor using WinMM |
| `midiksinfo.exe` | Provides device information |

# Configuration

By default, we shall include a configuration which has the standard settings and loopback endpoints defined. 

If the customer already has this file or the registry entry defined, we shall not overwrite either.

| File Name | Description |
| --- | --- |
| `%allusersprofile%\Microsoft\MIDI\Default.midiconfig.json` | Default MIDI configuration file. User has full permissions to this file. |

The default configuration file `Default.midiconfig.json` is in the source tree under the solution folder `Service\Default Configuration`

Installing this guarantees a set of user and app-visible loopback endpoints. These may be removed by the user using the MIDI Settings app.

`Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows MIDI Services`

| Value | Type | Description |
| --- | --- | --- |
| `CurrentConfig` | REG_SZ | contains just the name (not full path) of the configuration file. User has full permissions for this |
| `DefaultMidi1PortNaming` | REG_DWORD | `1` this signifies to use the default backwards-compatible port naming for MIDI 1.0 devices. User has full permissions to this value. |
| `UseLegacyMidi` | REG_DWORD | `0` Flag for the type of MIDI support to make available on the system|

