---
layout: doc
title: Windows MIDI Services App SDK Overview
---

Windows MIDI Services installs in two pieces
- The in-box service, MIDI 2 driver, and API which handle WinMM and WinRT MIDI 1.0 compatibility
- The out-of-band delivered SDK Runtime and Tools, which provides a richer customer experience due to the tools, and also provides apps with the required MIDI 2.0 features and capabilities, programmatic creation of endpoints (like Virtual Device endpoints), better enumeration support, and much more.

The second part is not shipped with Windows, and there are no short to medium-term plans to do so. We will evaluate in the future based upon frequency of changes, but this will not change how you distribute and use the runtime today.

## NuGet Package

This is for developers. The NuGet package contains the type metadata as well as the .NET projection.

Apps which want to use the SDK runtime compile against the WinRT `Microsoft.Windows.Devices.Midi2.winmd` metadata file. The runtime is the implementation of the types in that file, plus required WinRT bootstrapping code, and the COM endpoint to kick it all off. When installed, the interaction with WinRT is like that when talking to a native Windows SDK type that ships in-box.

> Currently, the NuGet package is delivered only through GitHub, and is not on NuGet.org. Download the NuGet package and place in a known location on your PC. Then add that location as a local repository for NuGet using your tools of choice. [Documentation here](https://learn.microsoft.com/nuget/hosting-packages/local-feeds).

### SDK Types

The [SDK namespaces and types in the metadata are documented here]({{"/sdk-reference/" | relative_url}}).

## Windows MIDI Services Runtime and Tools

The SDK runtime (implementation) is separate from the the WinMD/definitions. The runtime install is for both developers and MIDI users. It contains the implementation of the types from the NuGet package, and is centrally installed and maintained. The installer also contains the MIDI Console and MIDI Settings applications, as well as several utility applications, the PowerShell extensions, and more.

By keeping the runtime centralized, apps do not need to worry about servicing runtime bug fixes, security problems, and more. Microsoft is instead responsible for that work. It also enables us to make WinRT type initialization easier for apps and plugins, without requiring manifest files.

Microsoft is responsible for ensuring that the runtime is compatible with the NuGet packages [per SemVer Major/Minor/Patch version rules](https://semver.org/).

### Why not ship in Windows like the Windows.Devices.Midi namespace?

We initially started down that path, but due to the frequency of changes, and the limited app platforms we want to support initially (primarily desktops/laptops) we were advised to ship out of box, much like the Windows App SDK team does today. This enables us to focus our implementation without worrying about Day 1 support for everything Windows supports (Hololens and Xbox, for example), enabling us to both deliver v1 faster, and iterate more as new features are required to support MIDI 2.0 standards and customer/partner requests.

## Get started

To get started, you will need the compiler of your choice, the NuGet package for `Microsoft.Windows.Devices.Midi2` and if you are using C++, the `C++/WinRT` package. The latter is used to ingest the metadata and generate the necessary header files. C# developers will need the `C#/WinRT` package. Developers in other languages will need to speak to their compiler vendor to understand what tools they provide for generating WinRT projections.

Once set up, I recommend going to the samples area of the repo and learning how SDK and Service initialization happen, and then how to use the basic features of the SDK. The samples are all available [here](https://aka.ms/midisamples). The `cpp-winrt` folder contains the majority of the C++ code. The `cpp` folder contains example code which can detect the presence of Windows MIDI Services without using the SDK.
