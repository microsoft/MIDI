---
layout: kb
title: Application Backwards Compatibility
audience: everyone
description: Explains the API backwards compatibility for Windows MIDI Services.
categories:
  - Getting Started
  - Internals
---

**We want developers to move to Windows MIDI Services and away from WinMM, WinRT MIDI 1.0 and DirectMusic.** Every new MIDI feature and transport is being built in Windows MIDI Services and the new API. The older APIs get a few things they didn't have before, through our compatibility layer, but the point of that layer is to keep existing applications working on a PC that runs Windows MIDI Services. **We are not providing backwards compatibility for the DirectMusic MIDI APIs.**

The older MIDI APIs on Windows talk almost directly to MIDI 1.0 drivers. That's why only one application can use a driver at a time, unless the driver has extra code to keep track of each connected app. Windows MIDI Services is built around a Windows service instead, much like the audio system, and it uses a much faster way of moving data to and from the new MIDI 1.0 and MIDI 2.0 USB driver. That's what gives you multi-client access and good speed out of the box.

The service also translates between the MIDI 1.0 byte format and UMP, and between the high resolution MIDI 2.0 protocol and MIDI 1.0. Applications and devices don't see any of that happening.

## API Information

| API | What you can expect |
| --------------- | ----------------------------------- |
| Windows MIDI Services | This project. 100% of all supported features for MIDI 1.0 and MIDI 2.0, including multi-client. API/SDK uses UMP as its internal data format even for MIDI 1.0 devices. Transports and the service handle translation. |
| WinMM (Win32 API most apps use today) | Access to MIDI 1.0 and most MIDI 2.0 devices, at a MIDI 1.0 compatibility level only. |
| WinRT (MIDI API Introduced with Windows 10) | Access to MIDI 1.0 and most MIDI 2.0 devices, at a MIDI 1.0 compatibility level only. |
| DirectMusic | No compatibility planned. Not part of our testing. |
| Any 32 bit app | Other than WinMM which talks to the service from 32 bit apps by using WoW64, there is no compatibility planned. Note that development builds available on GitHub have no 32 bit application support. |


## Key Features

Each of the features listed is from the viewpoint of an application using the API when Windows MIDI Services is enabled.

| Feature | Windows MIDI Services API | Win32 WinMM API | WinRT MIDI 1.0 API | 
| ------- | ------------------------- | --------------- | ------------------ |
| **Basic Protocols** | | | |
| MIDI 2.0 Protocol (UMP) | ✅ | No | No |
| MIDI 1.0 Protocol (UMP) | ✅ | No | No |
| MIDI 1.0 Byte data format | No* | ✅ | ✅ |
| **Enhancements** | | | |
| Multi-Client MIDI (more than one app can use a MIDI endpoint) | ✅ | ✅ | ✅ |
| MIDI Timestamps (incoming) | ✅ | ✅*** | ✅*** |
| High-resolution 100ns unit absolute time MIDI Timestamps (incoming) | ✅ | No | No |
| MIDI Timestamps (outgoing, scheduled) | ✅ | No | No |
| Device connect/disconnect/update notifications | ✅ | No | ✅ |
| **Legacy API Features** | | | |
| Access WinMM .drv-style drivers (MIDI mappers, virtual synths, etc.) | No | ✅ | No |
| Use the in-box GS MIDI Synth | Preview** | ✅ | ✅ |
| **MIDI 2.0 Features** | | | |
| High resolution UMP messages | ✅ | No | No |
| Send/Receive with MIDI 1.0 USB devices | ✅ | ✅ | ✅ |
| Send/Receive with MIDI 2.0 USB devices | ✅ | ✅* | ✅* |
| Work with devices using MIDI CI | ✅ | ✅ | ✅ |
| Access Function Blocks and Endpoint Metadata | ✅ | No | No |
| **Built-in MIDI Transports** | | | |
| Send/Receive with Virtual MIDI Device | ✅ | ✅ | ✅ |
| Create Virtual MIDI Device | ✅ | No | No |
| Send/Receive with Basic and MIDI 2.0 Loopback MIDI Devices | ✅ | ✅ | ✅ |
| Create Basic and MIDI 2.0 Loopback MIDI Devices | ✅ | No | No |
| Send/Receive with Bluetooth MIDI 1.0 | Preview | Preview | Preview |
| Send/Receive with Network MIDI 2.0 | Preview | Preview | Preview |
| **Application Types** | | | |
| 64-bit Win32 (Desktop) App | ✅ | ✅ | ✅ |
| 64-bit UWP or Packaged App | ❓ | No | ✅ |
| 32-bit desktop app | No | ✅ | ✅ |

\* Messages are translated between the MIDI 1.0 byte format and the MIDI 2.0 UMP format whenever they need to be. The Windows MIDI Services API itself works only in UMP, and includes converters and helpers for applications that need MIDI 1.0 bytes.

\** The in-box Roland GS and General MIDI synthesizer is being rebuilt as a Windows MIDI Services transport. It's available now as a preview for developers, and ships in late 2026 with full MIDI 2.0 capabilities.

\*** Incoming timestamps have been available in the WinMM and WinRT MIDI 1.0 APIs since those APIs were introduced.

What the entries in the table mean:

- ✅ Supported
- Preview: it works today, but only in the preview releases for developers. It isn't in a consumer release of Windows yet.
- ❓ We're still investigating
- No: not supported

Arm64 and x86-64 ("x64" or "amd64") are equally supported by the 64 bit APIs. There is no support for 32-bit operating systems.

## Reverting to the old MIDI stack

See [How to change the API mode]({{ site.baseurl }}/kb/how-to-change-api-mode/) for how to put the PC back on the old WinMM implementation, without the MIDI service. That's for devices that use DirectMusic drivers, or anything else that isn't compatible with the new stack.
