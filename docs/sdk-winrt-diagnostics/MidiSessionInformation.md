---
layout: api_page
title: MidiSessionInformation
parent: Microsoft.Devices.Midi2.Diagnostics
has_children: false
---

# MidiSessionInformation

This class represents an open Windows MIDI Services session. 

## Properties

| Property | Description |
|---|---|
| `SessionId` | The generated internal `GUID` for the session |
| `ProcessId` | The process id for the session |
| `ProcessName` | The process name for the session, captured when the session was created |
| `SessionName` | The name of the session provided through the API |
| `StartTime` | The date and time the session was created |
| `Connections` | A list of `MidiSessionConnectionInformation` objects detailing the connections currently open for this session |

## Example

The Windows MIDI Services Console app uses the `MidiSessionInformation` `MidiSessionConnectionInformation` and the `MidiService` class to display active sessions.

![Console midi enum sessions](./console-enum-sessions.png)

In this case, you can see three open sessions. The process name and process id are on the left. The session name is in the text on the right, after the word "Session", and the start time is the date and time in green. Finally, the list of connections for each session is underneath the session information.

## IDL

[MidiSessionInformation IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiSessionInformation.idl)

