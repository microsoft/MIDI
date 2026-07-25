---
layout: kb
title: Application Backwards Compatibility
audience: everyone
description: Explains the API backwards compatibility for Windows MIDI Services.
---

**Our intention is for developers to begin adopting Windows MIDI Services in place of the older WinMM, WinRT, and (deprecated) DirectMusic APIs in their applications.** All new MIDI features, transports, and more are being implemented in Windows MIDI Services and the new API. A select number of features, slightly more than their current baseline, are available to WinMM and WinRT APIs through our backwards-compatibility shims, but this is simply to ensure existing applications continue to function on systems using Windows MIDI Services. **Please note that we are not providing backwards compatibility to support DirectMusic MIDI APIs.**

The existing MIDI APIs on Windows talk (almost) directly to MIDI 1.0 drivers through API calls. This is why only one application can use a driver at a time, unless the driver has had additional code implemented to keep track of each connected app. In Windows MIDI Services, the architecture is built around a central Windows Service, much like our audio system today. It also uses a much faster IO mechanism for communication with the new MIDI 1/MIDI 2 USB driver. This provides much more flexibility, including multi-client use, and good baseline speed with our new class driver.

The MIDI Service also includes internal translation between MIDI 1.0 data format and the UMP (MIDI 2.0) data format, as well as between the high resolution MIDI 2.0 protocol and MIDI 1.0. This is all transparent to applications and devices.

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
| Use the in-box GS MIDI Synth | No** | ✅ | ✅ |
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
| Send/Receive with Bluetooth MIDI 1.0 | After Initial Release | ❓ | ❓ |
| Send/Receive with Network MIDI 2.0 | After Initial Release | ✅ | ✅ |
| **Application Types** | | | |
| 64-bit Win32 (Desktop) App | ✅ | ✅ | ✅ |
| 64-bit UWP or Packaged App | ❓ | No | ✅ |
| 32-bit desktop app | No | ✅ | ❓ |

\* The Windows MIDI Services API includes converters and helpers to translate between MIDI 1.0 byte format and UMP in client apps.
\** Note that we are also investigating and experimenting with how to best incorporate the existing in-box Roland GS / General MIDI Synth into this architecture for apps using the new UMP-based API. We may add an additional transport in the future, specific to this or to another compatible synth.
\*** Incoming timestamps have been available in WinMM and WinRT MIDI 1.0 APIs since their introduction.

Arm64 and x86-64 ("x64" or "amd64") are both equally supported by the 64 bit APIs. There is no support for 32-bit operating systems.

✅ Feature is supported
❓We are investigating
\* Messages are translated between MIDI 1.0 protocol / data format and MIDI 2.0 protocol / UMP format

## Reverting to the old MIDI Stack

Please see other articles here for how to revert back to the old WinMM implementation, without using the MIDI Service, for cases where devices use DirectMusic drivers, or are otherwise not compatible with the new MIDI stack.
