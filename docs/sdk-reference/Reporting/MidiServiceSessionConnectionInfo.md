---
layout: sdk_reference_page
title: MidiSessionConnectionInformation
namespace: Microsoft.Windows.Devices.Midi2.Reporting
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Information about an open connection in the service
---

This class represents an open connection in a Windows MIDI Services session. This is an informational class only for reporting system-wide connection usage. 

## Properties

| Property | Description |
|---|---|
| `EndpointDeviceId` | The endpoint device id for the connection |
| `InstanceCount` | The number of instances of this connection which are open in the parent session |
| `EarliestConnectionTime` | The date and time the first instance of the connection was opened |
