---
layout: tools_page
title: PowerShell cmdlets for MIDI
tool: powershell
description: About the PowerShell cmdlets which enable scripting Windows MIDI Services
---

PowerShell support for MIDI is currently experimental and in-development. Expect minor changes in the cmdlets in the future.

These cmdlets require a minimum of PowerShell 7.4. [We recommend using the latest official version](https://learn.microsoft.com/powershell/scripting/install/installing-powershell-on-windows?view=powershell-7.5).

The cmdlets also rely on the .NET runtime, which is installed with the SDK Runtime and Tools installer. Currently, we use the latest version of .NET 8 desktop runtime, but that will change in the future when .NET 10 is available.

> The version of PowerShell which usually ships with Windows is currently the older Windows PowerShell. These cmdlets support the new cross-platform version of PowerShell. Please see the link above for how to install the latest 7.x version of PowerShell

<h2>What/Who are these for?</h2>

PowerShell is the primary command-line scripting language and environment in Windows, and the current version is also available on Linux and macOS. It is often used by system administrators to automate tasks to set up PCs, by developers to automate deployment or testing, and increasingly by technical Windows users to automate other common tasks. To learn more about PowerShell, see the documentation.

The PowerShell cmdlets for MIDI were created to enable advanced users on Windows to script MIDI. Here are some ideas of things which can be done:

- Synchronize setups between MIDI devices (mixers, lighting, more) in a large venue
- Set up patches and initial state in synthesizers, drum machines, sequencers, and more for a live performance
- Use input from a MIDI controller to send commands to another application, launch applications, send keystrokes, etc.

The cmdlets are reasonably fast, but we wouldn't expect someone to, for example, use them to create a high-performance MIDI sequencer, or other app which is real-time sensitive.

> The MIDI Console tool can also do the things the PowerShell cmdlets can do. The primary difference is the console tool opens (and closes) a new connection each time you do something like send a MIDI message. That is inefficient if you need to have a script which does many things with the same connection. If you only need to send a single message, using `midi endpoint send-message 0x25971234` is simple and fast. 

<h2>Startup/Shutdown cmdlets</h2>

The Windows MIDI Services SDK is based on WinRT, and ships separately from Windows. To allow for centralized servicing of the SDK, and avoid apps needing to ship manifests or their own components, the SDK uses detours-based activation and type resolution. This must be initialized and the components loaded into the process. 

In addition, the default startup mode of the Windows MIDI Services is to demand-start. That means it will not start up and begin enumerating endpoints, connecting to resources, etc. until it has been called. This is required so we don't slow down Windows startup for non-MIDI users. You can change this in the Services snap-in in Windows, or via command-line (including PowerShell) service management.

<h3>Start-Midi</h3>

This loads the SDK runtime and initializes WinRT redirection for the MIDI types. This step is required to resolve the types used by the cmdlets. This call also starts up the MIDI Service if it is not already running.

Be sure to call Stop-Midi when done. This pair of cmdlets should be called only once within a PowerShell process.

```pwsh
#Requires -Version 7.4
import-module WindowsMidiServices

$sdkinfo = Start-Midi | Write-Output
```

<h3>Stop-Midi</h3>

Call this as your last step in the script, to shut down the MIDI SDK WinRT redirection, and free up the resources it uses.


<h2>Enumeration cmdlets</h2>

One of the first things a MIDI tool or application typically does is list out all the available connections so that the software or its user can decide which endpoint(s) to create a connection to.

<h3>Get-MidiEndpointDeviceInfoList</h3>

Returns a list of all UMP MIDI Endpoints. Does not require an active MIDI Session.

```pwsh
#Requires -Version 7.4
import-module WindowsMidiServices

$sdkinfo = Start-Midi | Write-Output

# List all available endpoints. Enumeration functions do not require an active session.
Write-Host "Available MIDI Endpoints" -ForegroundColor Cyan
(Get-MidiEndpointDeviceInfoList) | Sort-Object -Property Name | Format-Table -AutoSize

# Cleanly shut down the SDK, stop WinRT activation redirection, etc.
Write-Host "Shutting down the SDK" -ForegroundColor Cyan
Stop-Midi
```

<h3>Get-MidiEndpointDeviceInfo</h3>

Given the endpoint device id as a parameter, returns details about that specific endpoint. Does not require an active MIDI Session.

```pwsh
# I'm using the default loopback that is created when you set up MIDI through the MIDI Settings app
$endpointDeviceId = "\\?\swd#midisrv#midiu_loop_a_default_loopback_a#{e7cce071-3c03-423f-88d3-f1045d02552b}"

# show some info about the device we're interested in
Write-Host "Endpoint we intend to connect to" -ForegroundColor Cyan
(Get-MidiEndpointDeviceInfo $endpointDeviceId)  | Format-List
```

<h3>Get-MidiEndpointGroups</h3>

This is not yet available, but will return the active MIDI Groups for an endpoint.

<h3>Get-MidiSessionList</h3>

Enumerates all the active MIDI Sessions in the service

```pwsh
# list all the active sessions
Write-Host "All active MIDI sessions" -ForegroundColor Cyan
(Get-MidiSessionList) | Sort-Object -Property Name | Format-Table -AutoSize
```


<h2>Session cmdlets</h2>

To send and receive messages with Windows MIDI Services, you must have an active session. The session is tracked in the service so that a MIDI users has visibility into the processes using MIDI on their PC. Most processes only need one MIDI Session, but they may open more than one if they need to group and manage connection usage by project, page, or similar.

Once you have an active session, you can open one or more connections to endpoints. Each active connection allocate resources on the client and in the service, so you only want to open connections you need, and ideally, only one connection per endpoint device id.

<h3>Start-MidiSession</h3>

Given the session name as a parameter, creates and activates a new MIDI Session. The returned object is required for calls which use a session, such as sending and receiving messages.

```pwsh
# create a new session so we can send and receive messages
$session = Start-MidiSession "Powershell Demo Session"
```

<h3>Stop-MidiSession</h3>

Ends the MIDI session

```pwsh
Stop-MidiSession $session
```

<h3>Open-MidiEndpointConnection</h3>

Given the session object and an endpoint device id, opens a connection to a MIDI UMP endpoint. The returned connection object is required for cmdlets which send and receive messages.

```pwsh
# open a connection to the endpoint
$connection = Open-MidiEndpointConnection $session $endpointDeviceId
```
<h3>Close-MidiEndpointConnection</h3>

Given session and connection objects, closes an open MIDI Endpoint Connection within the specified session.

```pwsh
Close-MidiEndpointConnection $session $connection
```

<h3>Send-MidiMessage</h3>

Given a connection object and an array of valid UMP message words (formatted as 32 bit integer values as complete UMPs), sends the single message. Do not include more than one valid UMP in the array of words.

A Timestamp value of 0 means to send the message immediately. Otherwise, a valid 64 bit integer derived from the MIDI Clock should be provided for scheduling a message in the near future.

```pwsh
# each sub-array is a complete MIDI UMP
$messages = (0x40905252, 0x02001111), (0x40805252, 0x02000000), 0x25971234

foreach ($message in $messages)
{
    Write-Host "Sending MIDI message" -ForegroundColor Cyan

    Send-MidiMessage $connection $message -Timestamp 0
}
```

<h3>Receiving Messages</h3>

To receive MIDI messages, use PowerShell's `Register-ObjectEvent` and background job support to handle the incoming messages. The `monitor-messages` sample includes the code for this.

The event handler args themselves are simplified from what direct WinRT clients receive. In this case, the data is supplied as a Timestamp field and an array of MIDI words as the `Words` field

```pwsh
$eventHandlerAction = {
    #Write-Host "Message Received"
    #Write-Host $EventArgs.Timestamp
    Get-MidiMessageInfo $EventArgs.Words
}

$job = Register-ObjectEvent -SourceIdentifier "OnMessageReceivedHandler" -InputObject $connection -EventName "MessageReceived" -Action $eventHandlerAction

# just spin until a key is pressed
do
{
    Receive-Job -Job $job
} until ([System.Console]::KeyAvailable)

# we don't do anything with the key here, but you could
$keyPressed = [System.Console]::ReadKey($true)

Write-Host
Write-Host "Key pressed. Shutting down ... "

Unregister-Event -SourceIdentifier "OnMessageReceivedHandler"
Stop-Job $job
Remove-Job $job
```

<h2>MIDI Utility cmdlets</h2>

In addition to enumeration, session management, and sending messages, there are some simple utility cmdlets

<h3>Get-MidiMessageInfo</h3>

Given a valid MIDI UMP message, this returns use-friendly information from decoding the supplied MIDI message

```pwsh
# each sub-array is a complete MIDI UMP
$messages = (0x40905252, 0x02001111), (0x40805252, 0x02000000), 0x25971234

foreach ($message in $messages)
{
    # this gets / displays information about the MIDI message we're sending
    Get-MidiMessageInfo $message | Format-List 
}
```



PowerShell [samples in the repo on GitHub](https://aka.ms/midisamples).