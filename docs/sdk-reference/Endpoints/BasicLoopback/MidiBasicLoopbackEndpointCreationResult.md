---
layout: sdk_reference_page
title: MidiBasicLoopbackEndpointCreationResult
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.BasicLoopback
library: Microsoft.Windows.Devices.Midi2.dll
type: struct
description: Response from the attempt to create a basic MIDI 1.0 loopback endpoint
---

This class represents the results of an attempt to create a runtime transient loopback endpoint

## Fields

| Property | Description |
|---|---|
| `Success` | True if the creation of both endpoints was a success |
| `AssociationId` | The GUID which associatiates the endpoint. This is used to provide structure similar to the MIDI 2.0 loopback endpoints. Provided during creation time. |
| `EndpointDeviceId` | The full endpoint device id `\\swd\...` for the endpoint created |
