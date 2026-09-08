---
layout: sdk_namespace_page
title: App SDK Support for Loopback Endpoints
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
description: Namespace for MIDI 1.0 loopback endpoint management
---

Types for configuring simple loopback endpoints.

## Samples

These create a single MIDI 1.0-style loopback endpoint at runtime, which is what an existing WinMM
or WinRT MIDI 1.0 application can talk to without any changes. Endpoints created this way are
transient, so remember to remove what you create.

* [C++/WinRT loopback-basic-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/loopback-basic-endpoints)
* [C# loopback-basic-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/loopback-basic-endpoints)
* [C++/WinRT loopback-basic-endpoints-winmm](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/loopback-basic-endpoints-winmm) additionally shows using the created endpoint from the WinMM API