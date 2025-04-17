---
layout: sdk_reference_page
title: MidiServicePingResponseSummary
namespace: Microsoft.Windows.Devices.Midi2.Diagnostics
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Overall response from pinging the MIDI Service
---

This class represents a summary of the ping attempts against the Windows service.

## Properties

| Property | Description |
|---|---|
| `Success` | True if the ping was a success |
| `FailureReason` | In case of a failure, this includes information about why the failure happened. |
| `TotalPingRoundTripMidiClock` | The total MIDI Clock time for all ping messages to be sent and received |
| `AveragePingRoundTripMidiClock` | Calculated average round trip time for ping messages |
| `Responses` | A list of all the responses for the ping messages |
