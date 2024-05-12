---
layout: api_page
title: MidiServicePingResponseSummary
parent: Microsoft.Devices.Midi2.Diagnostics
has_children: false
---

# MidiServicePingResponseSummary

This class represents a summary of the ping attempts against the Windows service.

## Properties

| Property | Description |
|---|---|
| `Success` | True if the ping was a success |
| `FailureReason` | In case of a failure, this includes information about why the failure happened. |
| `TotalPingRoundTripMidiClock` | The total MIDI Clock time for all ping messages to be sent and received |
| `AveragePingRoundTripMidiClock` | Calculated average round trip time for ping messages |
| `Responses` | A list of all the responses for the ping messages |

## IDL

[MidiServicePingResponseSummary IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiServicePingResponseSummary.idl)

