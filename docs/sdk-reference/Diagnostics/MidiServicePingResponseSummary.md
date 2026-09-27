---
layout: sdk_reference_page
title: MidiServicePingResponseSummary
namespace: Windows.Devices.Midi2.Diagnostics
type: runtimeclass
description: Overall response from pinging the MIDI Service
---

A summary of all the pings sent to the MIDI service. Most applications don't need it.

## Properties

| Property | Description |
|---|---|
| `Success` | True if the ping worked |
| `FailureReason` | If the ping failed, why |
| `TotalPingRoundTripMidiClock` | The total `MidiClock` time to send and receive all the pings |
| `AveragePingRoundTripMidiClock` | The average round trip time for one ping |
| `Responses` | A list of the answers to each ping |
