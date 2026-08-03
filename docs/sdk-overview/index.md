---
layout: doc
title: Windows MIDI Services App SDK Overview
---

# SDK Types

The [SDK namespaces and types in the metadata are documented here]({{"/sdk-reference/" | relative_url}}).

## Get started

To get started, you will need the compiler of your choice, until the types are in the official Windows SDK, the NuGet package for `Windows.Devices.Midi2` and if you are using C++, the `C++/WinRT` 3.x package. The latter is used to ingest the metadata and generate the necessary header files. C# developers will need the `C#/WinRT` package. Developers in other languages will need to speak to their compiler vendor to understand what tools they provide for generating WinRT projections.

Once set up, I recommend going to the samples area of the repo and learning how SDK and Service initialization happen, and then how to use the basic features of the SDK. The samples are all available [here](https://aka.ms/midisamples). The `cpp-winrt` folder contains the majority of the C++ code.
