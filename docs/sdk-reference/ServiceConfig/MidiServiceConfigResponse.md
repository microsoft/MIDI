---
layout: sdk_reference_page
title: MidiServiceConfigResponse
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Response from the service from a configuration attempt
---

What the service sent back after you sent it a configuration or a command. The details depend on the transport.

> **Important:** The JSON that goes to and from the service is an implementation detail, not a contract, and it can change. Don't build or edit this JSON by hand, and don't parse what comes back, unless you're writing a transport yourself.

## Properties

| Property | Description |
| --- | --- |
| `Status` | A `MidiServiceConfigResponseStatus` that says whether it worked |
| `ServiceErrorCode` | An error code from the transport, if it failed |
| `ServiceErrorMessage` | An error message from the service that people can read, if it failed |
| `ResponseJson` | A `JsonObject` with more details from the transport |
