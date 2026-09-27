---
layout: sdk_namespace_page
title: WinRT API Support for Loopback Endpoints
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
description: Namespace for MIDI 1.0 loopback endpoint management
---

Types for creating basic loopback endpoints. A basic loopback is one MIDI 1.0-style endpoint. Whatever is sent to it comes back out of the same endpoint, so one app can send and another can receive.

## Samples

These samples create one MIDI 1.0-style loopback endpoint while your app runs. Existing WinMM and WinRT MIDI 1.0 applications can use it without any changes. Endpoints made this way are temporary, so remember to remove the ones you create.

* [C++/WinRT loopback-basic-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/loopback-basic-endpoints)
* [C# loopback-basic-endpoints](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/loopback-basic-endpoints)
* [C++/WinRT loopback-basic-endpoints-winmm](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/loopback-basic-endpoints-winmm) also shows how to use the new endpoint from the WinMM API