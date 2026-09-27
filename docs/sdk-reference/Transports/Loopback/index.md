---
layout: sdk_namespace_page
title: WinRT API Support for Loopback Endpoints
namespace: Windows.Devices.Midi2.Transports.Loopback
library: Windows.Devices.Midi2.dll
description: Namespace for UMP loopback endpoint management
---

Types for creating loopback endpoint pairs. A loopback pair is two MIDI 2.0 endpoints, A and B, connected to each other. Whatever is sent to A comes in on B, and whatever is sent to B comes in on A.

## Samples

These samples create a pair of MIDI 2.0 endpoints connected to each other, while your app runs. Endpoints made this way are temporary. They last only while the service is running, and aren't saved in the configuration. Remember to remove the ones you create.

* [C++/WinRT loopback-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/loopback-endpoints)
* [C# loopback-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/loopback-endpoints)