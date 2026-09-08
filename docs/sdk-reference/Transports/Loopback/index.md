---
layout: sdk_namespace_page
title: App SDK Support for Loopback Endpoints
namespace: Windows.Devices.Midi2.Transports.Loopback
library: Windows.Devices.Midi2.dll
description: Namespace for UMP loopback endpoint management
---

Types for configuring simple loopback endpoints.

## Samples

These create a pair of MIDI 2.0 endpoints wired back to back, at runtime. Endpoints created this way
are transient: they live only while the service is running, and are not written to the configuration
file. Remember to remove what you create.

* [C++/WinRT loopback-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/loopback-endpoints)
* [C# loopback-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/loopback-endpoints)