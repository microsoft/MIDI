---
layout: sdk_reference_page
title: MidiServicePingResponse
namespace: Microsoft.Windows.Devices.Midi2.Diagnostics
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Response from a single ping message
---
This class represents a single ping message response. This is used to assess health and performance of the Windows service.

## Properties

| Property | Description |
|---|---|
| `SourceId` | Id used to track this ping source connection instance, in the case of multiple applications using the same ping endpoint |
| `Index` | Index of the ping |
| `ClientSendMidiTimestamp` | The time the client sent the ping message |
| `ServiceReportedMidiTimestamp` | The time the service reported receiving the ping message |
| `ClientReceiveMidiTimestamp` | The time the client received the ping response |
| `ClientDeltaTimestamp` | The delta between the client sending the message and receiving the response |

