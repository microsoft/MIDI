---
layout: sdk_namespace_page
title: App SDK Enumeration for Legacy APIs Overview
namespace: Windows.Devices.Midi2.Enumeration.Legacy
description: Integration with legacy MIDI 1.0 APIs like WinMM and WinRT MIDI 1.0
---

This namespaces contains types used for integration with legacy MIDI 1.0 APIs like WinMM and WinRT MIDI 1.0

## Samples

The legacy port watcher is the most direct answer to "I have a WinMM port list and I want the same
list, but with proper notifications instead of polling". It is a good first step when porting,
before restructuring around endpoints and groups.

* [C++/WinRT watch-midi1-ports](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/watch-midi1-ports)
* [C# watch-midi1-ports](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/watch-midi1-ports)
