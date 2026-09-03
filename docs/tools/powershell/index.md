---
layout: tools_page
title: PowerShell cmdlets for MIDI
tool: powershell
description: About the PowerShell cmdlets which enable scripting Windows MIDI Services
---

PowerShell support for MIDI is currently experimental and in-development. Expect minor changes in the cmdlets in the future.

These cmdlets require a minimum of PowerShell 7.6. [We recommend using the latest official version](https://learn.microsoft.com/powershell/scripting/install/installing-powershell-on-windows). Earlier 7.x releases run on an earlier .NET, which cannot load the module, so `Import-Module` refuses rather than failing later.

PowerShell itself is not installed by the SDK Runtime and Tools installer. The .NET desktop runtime the cmdlets need is, currently .NET 10.

> The version of PowerShell which usually ships with Windows is currently the older Windows PowerShell. These cmdlets support the new cross-platform version of PowerShell. Please see the link above for how to install the latest 7.x version of PowerShell

<h2>What/Who are these for?</h2>

PowerShell is the primary command-line scripting language and environment in Windows, and the current version is also available on Linux and macOS. It is often used by system administrators to automate tasks to set up PCs, by developers to automate deployment or testing, and increasingly by technical Windows users to automate other common tasks. To learn more about PowerShell, see the documentation.

The PowerShell cmdlets for MIDI were created to enable advanced users on Windows to script MIDI. Here are some ideas of things which can be done:

- Synchronize setups between MIDI devices (mixers, lighting, more) in a large venue
- Set up patches and initial state in synthesizers, drum machines, sequencers, and more for a live performance
- Use input from a MIDI controller to send commands to another application, launch applications, send keystrokes, etc.

The cmdlets are reasonably fast, but we wouldn't expect someone to, for example, use them to create a high-performance MIDI sequencer, or other app which is real-time sensitive.

> The MIDI Console tool can also do the things the PowerShell cmdlets can do. The primary difference is the console tool opens (and closes) a new connection each time you do something like send a MIDI message. That is inefficient if you need to have a script which does many things with the same connection. If you only need to send a single message, using `midi endpoint send-message 0x25971234` is simple and fast. 

<h2>Startup cmdlet</h2>

The default startup mode of Windows MIDI Services is to demand-start. That means it will not start up and begin enumerating endpoints, connecting to resources, etc. until it has been called. This is required so we don't slow down Windows startup for non-MIDI users. You can change this in the Services snap-in in Windows, or via command-line (including PowerShell) service management.

<h3>Start-Midi</h3>

This confirms Windows MIDI Services is available, starting the service if it is not already running. Every other cmdlet in the module performs the same check, so calling this first is optional. It is a useful first line in a script because it fails immediately, with a clear message, on a PC where Windows MIDI Services is not installed.

```pwsh
#Requires -Version 7.6
import-module WindowsMidiServices

Start-Midi
```

There is no matching shutdown cmdlet. Sessions and connections are released when you stop them, or when the PowerShell process ends.


<h2>Enumeration cmdlets</h2>

One of the first things a MIDI tool or application typically does is list out all the available connections so that the software or its user can decide which endpoint(s) to create a connection to.

<h3>Get-MidiEndpointDeviceInfo</h3>

With no parameters, returns all UMP MIDI Endpoints. With an endpoint device id, returns just that one. Does not require an active MIDI Session.

```pwsh
#Requires -Version 7.6
import-module WindowsMidiServices

# List all available endpoints. Enumeration functions do not require an active session.
Write-Host "Available MIDI Endpoints" -ForegroundColor Cyan
(Get-MidiEndpointDeviceInfo) | Sort-Object -Property Name | Format-Table -AutoSize
```

```pwsh
# I'm using the default loopback that is created when you set up MIDI through the MIDI Settings app
$endpointDeviceId = "\\?\swd#midisrv#midiu_loop_a_default#{e7cce071-3c03-423f-88d3-f1045d02552b}"

# show some info about the device we're interested in
Write-Host "Endpoint we intend to connect to" -ForegroundColor Cyan
(Get-MidiEndpointDeviceInfo $endpointDeviceId)  | Format-List
```

<h3>Get-MidiEndpointGroup</h3>

Returns the MIDI Groups an endpoint uses, taken from its declared function blocks when it has them, and from its group terminal blocks when it does not. Inactive function blocks are left out unless `-IncludeInactive` is supplied.

```pwsh
Get-MidiEndpointGroup -EndpointDeviceId $endpointDeviceId | Format-Table -AutoSize
```

<h3>Get-MidiLegacyPort</h3>

Returns the MIDI 1.0 ports which the older Windows MIDI APIs (WinMM and WinRT MIDI 1.0) see. These are created by Windows MIDI Services alongside the UMP endpoints they belong to, so this is how you map an endpoint to the port numbers an older application will show.

The ports can be listed in full, or narrowed by direction, by owning endpoint, by name, by container, or fetched individually by port device id.

```pwsh
# every MIDI 1.0 input port on the PC
Get-MidiLegacyPort -Flow MidiMessageSource | Format-Table -AutoSize

# only the ports belonging to one endpoint
Get-MidiLegacyPort -EndpointDeviceId $endpointDeviceId | Format-Table -AutoSize

# by the name an older application displays
Get-MidiLegacyPort -Name "MIDISPORT 2x2 In A"
```

<h3>Get-MidiSession</h3>

Enumerates all the active MIDI Sessions in the service, from every application, not just this one.

```pwsh
# list all the active sessions
Write-Host "All active MIDI sessions" -ForegroundColor Cyan
(Get-MidiSession) | Sort-Object -Property Name | Format-Table -AutoSize
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

<h2>System Exclusive cmdlets</h2>

MIDI 1.0 bytestream System Exclusive data, of the kind held in a `.syx` file, is carried over UMP as SysEx7 messages. These two cmdlets do the conversion and the flow control for you.

<h3>Send-MidiSystemExclusive</h3>

Sends a `.syx` file, or a byte array, to an open connection. Progress is reported through PowerShell's normal progress bar, and Ctrl+C cancels the transfer.

`-MessagesPerTransfer` and `-DelayBetweenTransfersMilliseconds` pace the data. Some devices, particularly older ones, need a slower pace to keep up.

```pwsh
Send-MidiSystemExclusive -Connection $connection -Path .\patches.syx -GroupIndex 0
```

<h3>Receive-MidiSystemExclusive</h3>

Captures System Exclusive data arriving on a group. Without `-Path` it writes one object per block of received bytes; with `-Path` it writes a `.syx` file instead, as the data arrives, so the file is complete even if the capture is interrupted.

Receiving is open ended, so it runs until `-MessageCount` messages have arrived, `-TimeoutSeconds` elapses, or Ctrl+C is pressed. Supply at least one of those unless you intend to stop it by hand.

```pwsh
# capture one complete message, or give up after 30 seconds
Receive-MidiSystemExclusive -Connection $connection -Path .\dump.syx -MessageCount 1 -TimeoutSeconds 30
```

<h2>Loopback endpoint cmdlets</h2>

Loopback endpoints are a pair of endpoints wired together, so what an application sends to one, another application receives from the other. Basic loopbacks are the single-endpoint MIDI 1.0 flavor, where an endpoint simply receives what it sends.

An endpoint created by these cmdlets is transient: it disappears when the service restarts. Supply `-SaveToConfiguration` to also write it to the Windows MIDI Services configuration file so it comes back.

<h3>New-MidiLoopback and New-MidiBasicLoopback</h3>

```pwsh
# the base name gets " (A)" and " (B)" appended, matching the other MIDI tools
New-MidiLoopback -BaseName "My Loopback" -SaveToConfiguration

# or name each side yourself
New-MidiLoopback -NameA "Sequencer Out" -NameB "Synth In"

New-MidiBasicLoopback -Name "My Basic Loopback"
```

<h3>Get-MidiLoopback and Get-MidiBasicLoopback</h3>

Lists the loopbacks the service currently has, including any created by other applications.

```pwsh
Get-MidiLoopback | Format-Table -AutoSize
```

<h3>Set-MidiLoopbackMute and Set-MidiBasicLoopbackMute</h3>

Mute is a property rather than an action, so it is set rather than toggled. `Mute` is not an approved PowerShell verb.

```pwsh
Set-MidiLoopbackMute -AssociationId $loopback.AssociationId -Muted $true
```

<h3>Remove-MidiLoopback and Remove-MidiBasicLoopback</h3>

Removes the endpoint from the running service. An entry saved in the configuration file is not affected, so a saved loopback returns when the service restarts.

```pwsh
Get-MidiLoopback | Where-Object { $_.EndpointA.Name -like 'My Loopback*' } | Remove-MidiLoopback
```

<h2>Network MIDI 2.0 cmdlets</h2>

<h3>Get-MidiNetworkAdvertisedHost</h3>

Lists the remote hosts this PC can currently see advertised on the network.

```pwsh
Get-MidiNetworkAdvertisedHost | Format-Table -AutoSize
```

<h3>Connect-MidiNetworkHost</h3>

Connects to a remote host, either one which was discovered, or one at a fixed address and port. A discovered host is matched on its advertisement, so the connection survives it moving to a new address; a direct address cannot do that, and is not retried automatically if it stops answering.

```pwsh
# connect to something which was discovered
Get-MidiNetworkAdvertisedHost |
    Where-Object { $_.DeviceName -eq 'BomeBox' } |
    Connect-MidiNetworkHost -SaveToConfiguration

# connect to a fixed address
Connect-MidiNetworkHost -HostNameOrAddress 192.168.1.243 -Port 33327 -SaveToConfiguration
```

<h3>Disconnect-MidiNetworkHost</h3>

Disconnects by client identifier, by the device id of the host it was matched to, or by address and port.

```pwsh
Disconnect-MidiNetworkHost -ClientId $response.ClientId
Disconnect-MidiNetworkHost -HostNameOrAddress 192.168.1.243 -Port 33327
```

<h3>Get-MidiNetworkConfiguredHost and Get-MidiNetworkConfiguredClient</h3>

Hosts are what this PC advertises for remote devices to connect to. Clients are the connections this PC makes out to remote hosts. A client is reported even when it is not connected, so `EntryState` is what says whether it is usable.

```pwsh
Get-MidiNetworkConfiguredHost | Format-Table -AutoSize
Get-MidiNetworkConfiguredClient | Format-Table -AutoSize
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