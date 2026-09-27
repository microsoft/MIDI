---
layout: sdk_reference_page
title: MidiServicePingResponse
namespace: Windows.Devices.Midi2.Diagnostics
type: runtimeclass
description: Response from a single ping message
---
The answer to one ping message. It's used to check how well the MIDI service is working. Most applications don't need it.

## Properties

| Property | Description |
|---|---|
| `SourceId` | An id for the connection that sent the ping, so answers don't get mixed up when several applications ping at once |
| `Index` | Which ping this is |
| `ClientSendMidiTimestamp` | When your application sent the ping |
| `ServiceReportedMidiTimestamp` | When the service says it received the ping |
| `ClientReceiveMidiTimestamp` | When your application got the answer |
| `ClientDeltaTimestamp` | The time between sending the ping and getting the answer |

