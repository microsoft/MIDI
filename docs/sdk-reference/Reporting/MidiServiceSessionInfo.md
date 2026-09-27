---
layout: sdk_reference_page
title: MidiServiceSessionInfo
namespace: Windows.Devices.Midi2.Reporting
type: runtimeclass
description: Reporting information about a single open session in the MIDI Service
---

One open Windows MIDI Services session.

## Properties

| Property | Description |
|---|---|
| `SessionId` | The GUID the service gave the session |
| `ProcessId` | The id of the process that owns the session |
| `ProcessName` | The name of that process, saved when the session was created |
| `SessionName` | The name the application gave the session |
| `StartTime` | When the session was created |
| `Connections` | A list of `MidiServiceSessionConnectionInfo`, one for each connection the session has open now |

## Example

The MIDI Console uses `MidiServiceSessionInfo`, `MidiServiceSessionConnectionInfo`, and `MidiReporting` to show the open sessions.

![Console midi enum sessions]({{ site.baseurl }}/assets/images/console-enum-sessions.png)

This shows three open sessions. The process name and process id are on the left. The session name is on the right, after the word "Session," and the start time is the date and time in green. Each session's connections are listed under it.
