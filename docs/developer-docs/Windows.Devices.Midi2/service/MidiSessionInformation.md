---
layout: api_page
title: MidiSessionInformation
parent: Service
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiSessionInformation

This class represents an open Windows MIDI Services session. 

## Static Functions

| Property | Description |
|---|---|
| `SessionId` | The generated internal `GUID` for the session |
| `ProcessId` | The process id for the session |
| `ProcessName` | The process name for the session, captured when the session was created |
| `SessionName` | The name of the session provided through the API |
| `StartTime` | The date and time the session was created |
| `Connections` | A list of `MidiSessionConnectionInformation` objects detailing the connections currently open for this session |

## IDL

[MidiSessionInformation IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiSessionInformation.idl)

