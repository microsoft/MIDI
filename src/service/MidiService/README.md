# MIDI Service

## Flow

Initial Connection Flow

* Server spins up the main connection named pipe.
* Client connects to main connection named pipe
* Server spins up thread with dedicated session pipe
* Client receives a dedicated session pipe name and session ID
* Client immediately disconnects from main connection pipe, freeing it up for other apps
* Client connects to dedicated session pipe

Session Management

* Client connects to dedicated session pipe
* Client requests a new session
* Server returns session details

Session Termination

* Client sends session destroy message
* Server tears down everything for that session, including graph hooks, threads, etc.
* Client code, if still running, tears down local session object and everything hanging off of it

Enumerate Devices

* Client sends Device Enumeration message to dedicated session pipe

## Install and Management

During dev, you'll need to manually stop/start/install/uninstall the MIDI services. You'll need to publish the exe first, and then use the path to that exe in the steps below.

The below steps work from an Administrator command prompt or Administrator PowerShell. I recommend the Windows Terminal app.

### Manual Install

I recommend using the installer for at least the first install. It creates the required folders and sets permissions on them so the service can access the required data.

```
sc create "MIDI Service" binpath="f:\ull\path\to\MidiService.exe"
```

example:

```
sc create "MIDI Service" binpath="D:\peteb\Documents\GitHub\microsoft\midi\src\api\MidiServices\MidiService\bin\Release\net7.0-windows10.0.20348.0\win-x64\publish\MidiService.exe"
```

Optional step

```
sc description "MIDI Service" "In-development version of Windows MIDI Services"
```

### Manual Start and Stop

You can use the Services snap-in, or handle via PowerShell / CMD. You need to make sure you stop the service before overwriting its exe.

```
sc start "MIDI Service"

sc stop "MIDI Service"
```

### Manual Removal

```
sc delete "MIDI Service"
```

### Using the PowerShell Scripts

All of the above steps are rolled up into the following PowerShell scripts:

| Script | Function
| ----------------------- | -------------------------------------------------------- |
| .\delete-service.ps1 | Stop and delete the service. This can take a few minutes to actually happen after the script has been run. |
| .\start-service.ps1 | Create and start the service |
| .\list-midi-pipes.ps1 | Output a list of all the MIDI named pipes that are currently open |
| .\Constants.ps1 | This is where you set the service path for your own machine |

Do not check in your own versions of Constants.ps1.

## Named Pipes

This server uses named pipes to communicate with the clients.

### Debugging named pipes

To see all the open named pipes in the system, open PowerShell and type:

```
[System.IO.Directory]::GetFiles("\\.\\pipe\\")
```

To scope to just the ones for MIDI, use a wildcard search for the prefix defined in the MidiServiceConstants file.

```
[System.IO.Directory]::GetFiles("\\.\\pipe\\", "midi*")
```

Look for the ones with the name in the Constants.cs file here.

### More info

Info anyone working on the server may find useful information about pipes

* [General Win32 Named Pipes info](https://docs.microsoft.com/windows/win32/ipc/pipes)
* [System.IO.Pipes Namespace for .NET 7](https://docs.microsoft.com/dotnet/api/system.io.pipes?view=net-7.0)
* [Win32 Multithreaded Pipe Server example](https://docs.microsoft.com/windows/win32/ipc/multithreaded-pipe-server)
* [Win32 Named Pipe Client example](https://docs.microsoft.com/windows/win32/ipc/named-pipe-client?redirectedfrom=MSDN)

And these references on shared memory and memory-mapped files


## Serialization

This project uses Google Protobuf (through Marc Gravell's Protobuf-net) 'for fast serialization of types to pipes and other shared memory. This is only used in cases where the end-user is not expected to view or edit the data directly. Binary serialization using the built-in Binary formatter in .NET is both unsafe and deprecated.

For end-user editing, for example the setup/config files, we use System.Text.Json serialization. 

* [Protobuf-net](https://github.com/protobuf-net/protobuf-net)
* [Protobuf-net Pages](https://protobuf-net.github.io/protobuf-net/)
* [Nice blog post series by Wade Smith on using Protobuf with C#](https://dotnetcoretutorials.com/2022/01/13/protobuf-in-c-net-part-1-getting-started/)
* [Nuget package source](https://www.nuget.org/packages/protobuf-net/)
* [Stack Overflow questions](https://stackoverflow.com/questions/tagged/protobuf-net)

For the Google implementations, just for reference. We don't use this version here.

* [Protobuf C#](https://github.com/protocolbuffers/protobuf/tree/main/csharp)
