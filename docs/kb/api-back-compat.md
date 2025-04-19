---
layout: kb
title: Application Backwards Compatibility
audience: everyone
description: Explains the API backwards compatibility for Windows MIDI Services.
---

**Our intention is for developers to begin adopting Windows MIDI Services in place of the older WinMM, WinRT, and (deprecated) DirectMusic APIs in their applications.** All new MIDI features, transports, and more will be implemented in Windows MIDI Services and the new API. A select number of features, slightly more than their current baseline, will be available to WinMM and WinRT APIs through our backwards-compatibility shims and abstractions, but this is simply to ensure existing applications continue to function on systems using Windows MIDI Services. **Please note that we are not providing backwards compatibility to support DirectMusic MIDI APIs.**

The existing MIDI APIs on Windows talk (almost) directly to MIDI 1.0 drivers through kernel calls. In Windows MIDI Services, the architecture is built around a central Windows Service, much like our audio system today. It also uses a much faster IO mechanism for communication with the USB driver vs what our MIDI 1.0 API uses today. This provides much more flexibility, including the potential for multi-client use, and good baseline speed with our new class driver. We are working on shims and abstractions which will allow some of the existing MIDI 1.0 APIs to talk to the service rather than directly to the driver.

Here is where we currently stand with planned backwards compatibility. Backwards compatibility for WinMM and WinRT APIs will be a post-1.0 feature, but shortly after that first release.

| API | What you should expect |
| --------------- | ----------------------------------- |
| Windows MIDI Services | This project. 100% of all supported features for MIDI 1.0 and MIDI 2.0, including multi-client. API/SDK uses UMP as its internal data format even for MIDI 1.0 devices. Transports and the service handle translation. |
| WinMM (Win32 API most apps use today) | Access to MIDI 1.0 and most MIDI 2.0 devices, at a MIDI 1.0 compatibility level only. |
| WinRT (MIDI API Introduced with Windows 10) | Access to MIDI 1.0 and most MIDI 2.0 devices, at a MIDI 1.0 compatibility level only. |
| DirectMusic | No compatibility planned. Not part of our testing. |
| Any 32 bit API | Other than WinMM which talks to the service from 32 bit apps by using WoW64, there is no compatibility planned. Note that development builds available on GitHub have no 32 bit application support. |

> Note that we are also investigating and experimenting with how to best incorporate the existing in-box Roland GS / General MIDI Synth into this architecture for apps using the new UMP-based API. We may add an additional transport in the future, specific to this or to another compatible synth.

Here's the current short-term plan for when the API redirection is in-box. Each of the features listed is from the viewpoint of an application using the API.

| Feature | Windows MIDI Services API | Win32 WinMM API | WinRT MIDI 1.0 API | 
| ------- | ------------------------- | --------------- | ------------------ |
| **Basic Protocols** | | | |
| MIDI 2.0 Protocol (UMP) | ✅ | No | No |
| MIDI 1.0 Protocol (UMP) | ✅ | No | No |
| MIDI 1.0 Byte data format | No | ✅ | ✅ |
| **Enhancements** | | | |
| Multi-Client MIDI (more than one app can use a MIDI endpoint) | ✅ | ✅ | ✅ |
| MIDI Timestamps (incoming) | ✅ | No | No |
| MIDI Timestamps (outgoing, scheduled) | ✅ | No | No |
| Device connect/disconnect notifications | ✅ | No | ✅ |
| **MIDI 2.0 Features** | | | |
| High resolution UMP messages | ✅ | No | No |
| Send/Receive with MIDI 1.0 USB devices | ✅ | ✅ | ✅ |
| Send/Receive with MIDI 2.0 USB devices | ✅ | ✅* | ✅* |
| Work with devices using MIDI CI | ✅ | ✅ | ✅ |
| Access Function Blocks and Endpoint Metadata | ✅ | No | No |
| **Built-in MIDI Transports** | | | |
| Send/Receive with Virtual MIDI Device | ✅ | ✅ | ✅ |
| Create Virtual MIDI Device | ✅ | No | No |
| Send/Receive with Loopback MIDI Device | ✅ | ✅ | ✅ |
| Create Loopback MIDI Device | ✅ | No | No |
| Send/Receive with Bluetooth MIDI 1.0 | After Initial Release | ❓ | ❓ |
| Send/Receive with Network MIDI 2.0 | After Initial Release | ✅ | ✅ |
| **Application Types** | | | |
| 64-bit Win32 (Desktop) App | ✅ | ✅ | ✅ |
| 64-bit UWP or Packaged App | ❓ | No | ✅ |
| 32-bit desktop app | No | ✅ | ❓ |

Arm64 and x86-64 ("x64" or "amd64") are both equally supported by the 64 bit APIs. There is no support for 32-bit operating systems.

✅ Feature is supported
❓We are investigating
\* Messages are translated between MIDI 1.0 protocol / data format and MIDI 2.0 protocol / UMP format