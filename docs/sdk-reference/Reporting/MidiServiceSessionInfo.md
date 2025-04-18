---
layout: sdk_reference_page
title: MidiServiceSessionInfo
namespace: Microsoft.Windows.Devices.Midi2.Reporting
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Reporting information about a single open session in the MIDI Service
---

This class represents an open Windows MIDI Services session. 

## Properties

| Property | Description |
|---|---|
| `SessionId` | The generated internal `GUID` for the session |
| `ProcessId` | The process id for the session |
| `ProcessName` | The process name for the session, captured when the session was created |
| `SessionName` | The name of the session provided through the API |
| `StartTime` | The date and time the session was created |
| `Connections` | A list of `MidiServiceSessionConnectionInfo` objects detailing the connections currently open for this session |

## Example

The Windows MIDI Services Console app uses the `MidiServiceSessionInfo` `MidiServiceSessionConnectionInfo` and the `MidiReporting` class to display active sessions.

![Console midi enum sessions](/assets/images/console-enum-sessions.png)

In this case, you can see three open sessions. The process name and process id are on the left. The session name is in the text on the right, after the word "Session", and the start time is the date and time in green. Finally, the list of connections for each session is underneath the session information.
