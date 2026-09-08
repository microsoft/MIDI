# Using Windows MIDI Services from PowerShell

PowerShell is the preferred scripting language for Windows. It enables developers and system administrators to automate tasks without the overhead of writing a full compiled program.

Requirements: PowerShell 7.6+ in Windows, .NET 10+ Desktop Runtime. You must also have PowerShell scripting enabled in Settings > For Developers

Note that there are older versions of PowerShell, typically pre-installed with Windows. We specifically require version 7.6+ of PowerShell (pwsh), not the older Windows PowerShell.

## Samples

| Sample | Description |
| -------| ----------- |
| [Enum Endpoints](enum-endpoints.ps1) | Lists the available MIDI endpoints |
| [Enum Groups](enum-groups.ps1) | Lists the groups on each endpoint. Groups are the closest thing to a WinMM "port" |
| [Enum Legacy Ports](enum-legacy-ports.ps1) | Lists the MIDI 1.0 ports that WinMM and WinRT MIDI 1.0 applications see, and maps them back to their endpoints |
| [Enum Sessions](enum-sessions.ps1) | Lists all active sessions |
| [Loopback Basic Endpoints](loopback-basic-endpoints.ps1) | Creates, mutes and removes a MIDI 1.0-style loopback endpoint at runtime |
| [Loopback Endpoints](loopback-endpoints.ps1) | Creates, mutes and removes a pair of MIDI 2.0 loopback endpoints at runtime |
| [Monitor Messages](monitor-messages.ps1) | Receives incoming MIDI messages |
| [Network Info](network-info.ps1) | Reports on Network MIDI 2.0 advertised hosts, configured hosts and clients. Read-only |
| [Send Messages](send-messages.ps1) | Sends MIDI messages to an endpoint |
| [SysEx Receive](sysex-receive.ps1) | Receives a System Exclusive message, as objects or straight to a `.syx` file |
| [SysEx Send](sysex-send.ps1) | Sends System Exclusive data from a `.syx` file or from a byte array |
| [Full Test](test-midi.ps1) | Shows a large number of the PowerShell scripting features |

Several samples pair up. Run `sysex-receive.ps1` in one window and `sysex-send.ps1` in another,
both pointed at the same loopback endpoint, to see a System Exclusive transfer work with no
hardware attached.

## Before running

These samples need the `WindowsMidiServices` PowerShell module, which is installed as part of
Windows MIDI Services from the [releases page](https://github.com/microsoft/MIDI/releases). The
installer adds the module to your `PSModulePath`, so `import-module WindowsMidiServices` will find
it without any further setup.

The samples which create loopback endpoints create them as *transient*: they exist only while the
service is running, and nothing is written to your configuration file. Each of those samples
removes what it created. If you stop one early, restart the MIDI service to clear anything left
behind, or change the unique id in the script.

## Background information

* [Install PowerShell](https://aka.ms/powershell-release?tag=stable)

## Documentation

* [Documentation available here.](https://microsoft.github.io/MIDI/tools/powershell/)
