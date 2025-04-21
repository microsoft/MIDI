---
layout: doc
title: Windows MIDI Services App SDK Overview
---

Windows MIDI Services installs in two pieces
- The in-box service, MIDI 2 driver, and API which handle WinMM and WinRT MIDI 1.0 compatibility
- The out-of-band delivered SDK Runtime and Tools, which provides a richer customer experience due to the tools, and also provides apps with the required MIDI 2.0 features and capabilities, programmatic creation of endpoints (like Virtual Device endpoints), better enumeration support, and much more.

The second part is not shipped with Windows, and there are no short to medium-term plans to do so. We will evaluate in the future based upon frequency of changes, but this will not change how you distribute and use the runtime today.

## NuGet Package

Apps which want to use the SDK runtime compile against the WinRT `Microsoft.Windows.Devices.Midi2.winmd` metadata file. The runtime is the implementation of the types in that file, plus required WinRT bootstrapping code, and the COM endpoint to kick it all off. When installed, the interaction with WinRT is like that when talking to a native Windows SDK type that ships in-box.

## Separate Runtime

The SDK runtime (implementation) is separate from the the WinMD/definitions.

### Why not let apps ship the whole package?

By keeping the runtime centralized, apps do not need to worry about servicing runtime bug fixes, security problems, and more. Microsoft is instead responsible for that work.

### Why not ship in Windows like the Windows.Devices.Midi namespace?

We initially started down that path, but due to the frequency of changes, and the limited app platforms we want to support initially (primarily desktops/laptops) we were advised to ship out of box, much like the Windows App SDK team does today. This enables us to focus our implementation without worrying about Day 1 support for everything Windows supports (Hololens and Xbox, for example), enabling us to both deliver v1 faster, and iterate more as new features are required to support MIDI 2.0 standards and customer/partner requests.

## SDK Types

The [SDK namespaces and types are documented here]({{"/sdk-reference/" | relative_url}}).
