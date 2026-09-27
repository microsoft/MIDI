---
layout: sdk_reference_page
title: MidiServiceSessionConnectionInfo
namespace: Windows.Devices.Midi2.Reporting
type: runtimeclass
description: Information about an open connection in the service
---

One open connection in a Windows MIDI Services session. It's only for reporting which connections are open across the PC.

## Properties

| Property | Description |
|---|---|
| `EndpointOrPortDeviceId` | The device id for the connection. It's a UMP endpoint id or a MIDI 1.0 port id, because a session can have both open |
| `InstanceCount` | How many of this connection the session has open |
| `EarliestConnectionTime` | When the first of those connections was opened |
