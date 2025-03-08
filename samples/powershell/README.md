# Using Windows MIDI Services from PowerShell

PowerShell is the preferred scripting language for Windows. It enables developers and system administrators to automate tasks without the overhead of writing a full compiled program.

The 

Requirements: PowerShell 7.4+ in Windows, .NET 8+ Desktop Runtime. You must also have PowerShell scripting enabled in Settings > For Developers

Note that there are older versions of PowerShell, typically pre-installed with Windows. We specifically require version 7.4+ of PowerShell (pwsh), not the older Windows PowerShell.

## Samples

| Sample | Description |
| -------| ----------- |
| [Enum Endpoints](enum-endpoints.ps1) | Shows how to list available MIDI endpoints |
| [Enum Sessions](enum-sessions.ps1) | Lists all active sessions |
| [Send Messages](send-messages.ps1) | Demonstrates sending MIDI messages to an endpoint |
| [Full Test](test-midi.ps1) | Shows a large number of the PowerShell scripting features |

We recommend you load the entire solution in Visual Studio, as that is how the NuGet package configuration is set up. Change the sample you run by setting that project as startup.

Before running, you will need to have installed the Windows MIDI Services App SDK Runtime. You will also need the NuGet package that these projects use, in a location on your PC configured as a NuGet package source. The runtime and NuGet package are available in the official releases.

## Background information

* [Install PowerShell](https://aka.ms/powershell-release?tag=stable)

