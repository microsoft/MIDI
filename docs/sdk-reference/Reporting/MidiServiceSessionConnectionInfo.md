---
layout: sdk_reference_page
title: MidiServiceSessionConnectionInfo
namespace: Windows.Devices.Midi2.Reporting
type: runtimeclass
description: Information about an open connection in the service
---

This class represents an open connection in a Windows MIDI Services session. This is an informational class only for reporting system-wide connection usage. 

## Properties

| Property | Description |
|---|---|
| `EndpointOrPortDeviceId` | The device id for the connection. This is a UMP endpoint id or a MIDI 1.0 port id, because a session can hold both |
| `InstanceCount` | The number of instances of this connection which are open in the parent session |
| `EarliestConnectionTime` | The date and time the first instance of the connection was opened |
