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

Sending Messages

* Client uses session to open endpoint (this should be kept open for as long as the application may need to use the endpoint, as opening/closing are more expensive operations)
* Client sends MIDI messages using SDK
* SDK sends the messages using cross-process shared memory queues
* Service processes messages and routes to correct endpoint
* Client optionally disconnects from endpoint

Receiving Messages

* As per above, client opens endpoint
* Endpoint sends messages to service using local memory
* Service copies messages to all listening client queues
* Client is notified that there are new messages waiting
* Client processes messages

## Install and Management

During dev, you'll need to manually stop/start/install/uninstall the MIDI services. You'll need to publish the exe first, and then use the path to that exe in the steps below.

The below steps work from an Administrator command prompt or Administrator PowerShell. I recommend the Windows Terminal app.

### Manual Install

I recommend using the installer for at least the first install. It creates the required folders and sets permissions on them so the service can access the required data. After that, you can use sc.exe.

```PowerShell
sc.exe create "MIDI Service" binpath="f:\ull\path\to\MidiService.exe"
```

example:

```PowerShell
sc.exe create "MIDI Service" binpath="D:\peteb\Documents\GitHub\microsoft\midi\src\api\MidiServices\MidiService\bin\Release\net7.0-windows10.0.20348.0\win-x64\publish\MidiService.exe"
```

Optional step

```PowerShell
sc.exe description "MIDI Service" "In-development version of Windows MIDI Services"
```

### Manual Start and Stop

You can use the Services snap-in, or handle via PowerShell / CMD. You need to make sure you stop the service before overwriting its exe.

```PowerShell
sc.exe start "MIDI Service"

sc.exe stop "MIDI Service"
```

### Manual Removal

```PowerShell
sc.exe delete "MIDI Service"
```

### Using the PowerShell Scripts

All of the above steps are rolled up into the following PowerShell scripts:

| Script | Function
| ----------------------- | -------------------------------------------------------- |
| .\delete-service.ps1 | Stop and delete the service. This can take a few moments to actually happen after the script has been run. If it takes too long, it may be that the service has code in a tight loop, and needs to be manually killed in task manager. The tight loop then needs to be fixed. |
| .\start-service.ps1 | Create and start the service |
| .\list-midi-pipes.ps1 | Output a list of all the MIDI named pipes that are currently open |
| .\Constants.ps1 | This is where you set the service path for your own machine |

**Do not check in your own versions of Constants.ps1.**

## Named Pipes and Memory-mapped Files

This server uses named pipes and memory-mapped files to communicate with the clients. The initial connections are done via pipes which are easier to manage, but are slower. All MIDI messages transfers, and enumeration, are done via memory-mapped files which require more code to manage, but are quite a bit faster.

For pipes, serialization is handled using Protobuf.net. This may change, but there's additional complexity when using System.Text.Json for pipes, and the BinaryFormatter/Serializer is no longer recommended for new projects. If the number of messages stays small, it will be reasonable to implement custom serialization and eliminate the dependency.

### Debugging named pipes

To see all the open named pipes in the system, open PowerShell and type:

```PowerShell
[System.IO.Directory]::GetFiles("\\.\\pipe\\")
```

To scope to just the ones for MIDI, use a wildcard search for the prefix defined in the MidiServiceConstants file in the Protocol project.

```PowerShell
[System.IO.Directory]::GetFiles("\\.\\pipe\\", "midi*")
```

### Viewing all memory-mapped files

Use [Process Explorer](https://docs.microsoft.com/sysinternals/downloads/process-explorer) to view processes and their open memory-mapped files. To do this, run Process Explorer, and then select View > Show Loweer Pane and then View > Lower Pane View > Handles

### More info

Info anyone working on the server may find useful information about pipes

* [General Win32 Named Pipes info](https://docs.microsoft.com/windows/win32/ipc/pipes)
* [System.IO.Pipes Namespace for .NET 7](https://docs.microsoft.com/dotnet/api/system.io.pipes?view=net-7.0)
* [Win32 Multithreaded Pipe Server example](https://docs.microsoft.com/windows/win32/ipc/multithreaded-pipe-server)
* [Win32 Named Pipe Client example](https://docs.microsoft.com/windows/win32/ipc/named-pipe-client?redirectedfrom=MSDN)

And these references on shared memory and memory-mapped files

* [Memory-mapped files](https://docs.microsoft.com/dotnet/standard/io/memory-mapped-files)

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

## Genaral service info

The MIDI Service creates a named event log "MIDI Service" under the Applications and Services Logs branch of the Windows event log. You'll find all application-generated events there. SCM, abends, and other events will still show up in the main Windows Logs > Application event log.

