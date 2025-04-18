---
layout: sdk_reference_page
title: MidiLoopbackEndpointDefinition
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Loopback
library: Microsoft.Windows.Devices.Midi2.dll
type: struct
description: The information supplied when creating a loopback endpoint pair
---

## Fields

| Property | Description |
|---|---|
| `Name` | The name of the endpoint. |
| `UniqueId` | A short unique identifier for this endpoint. This is used in creating the id. Keep to 32 characters or fewer (32 characters is the length of a no-symbols GUID). If, when combined with the generated loopback A/B differentiator prefix, this id is not unique among all loopback endpoints, endpoint creation will fail. |
| `Description` | Optional description for the endpoint |
