---
layout: sdk_reference_page
title: MidiLoopbackEndpointCreationResult
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Loopback
library: Microsoft.Windows.Devices.Midi2.dll
type: struct
description: Response from the attempt to create a loopback endpoint pair
---

This class represents the results of an attempt to create runtime transient loopback endpoints

## Fields

| Property | Description |
|---|---|
| `Success` | True if the creation of both endpoints was a success |
| `AssociationId` | The GUID which associatiates the two endpoints. Provided during creation time. |
| `EndpointDeviceIdA` | The full endpoint device id `\\swd\...` for the endpoint identified as the "A" side of the loopback |
| `EndpointDeviceIdB` | The full endpoint device id `\\swd\...` for the endpoint identified as the "B" side of the loopback |
