---
layout: page
title: API Backwards Compatibility
parent: For Developers
---

# API Backwards Compatibility

**Our intention is for developers to begin adopting Windows MIDI Services in place of the older WinMM, WinRT, and (deprecated) DirectMusic APIs in their applications.** All new MIDI features, transports, and more will be implemented in Windows MIDI Services and the new API. A select number of features, slightly more than their current baseline, will be available to WinMM and WinRT APIs through our backwards-compatibility shims and abstractions, but this is simply to ensure existing applications continue to function on systems using Windows MIDI Services. **Please note that we are not providing backwards compatibility to support DirectMusic MIDI APIs.**

The existing MIDI APIs on Windows talk (almost) directly to MIDI 1.0 drivers through kernel calls. In Windows MIDI Services, the architecture is built around a central Windows Service, much like our audio system today. It also uses a much faster IO mechanism for communication with the USB driver vs what our MIDI 1.0 API uses today. This provides much more flexibility, including the potential for multi-client use, and good baseline speed with our new class driver. We are working on shims and abstractions which will allow some of the existing MIDI 1.0 APIs to talk to the service rather than directly to the driver.

Here is where we currently stand with planned backwards compatibility. Backwards compatibility for WinMM and WinRT APIs will be a post-1.0 feature, but shortly after that first release.

| API | What you should expect |
| --------------- | ----------------------------------- |
| Windows MIDI Services | This project. 100% of all supported features for MIDI 1.0 and MIDI 2.0, including multi-client. API/SDK uses UMP as its internal data format even for MIDI 1.0 devices. Transports and the service handle translation. |
| WinMM (Win32 API most apps use today) | Access to MIDI 1.0 and most MIDI 2.0 devices, at a MIDI 1.0 compatibility level only. It is possible we will add multi-client support here after our initial release. |
| WinRT (MIDI API Introduced with Windows 10) | Access to MIDI 1.0 and most MIDI 2.0 devices, at a MIDI 1.0 compatibility level only. It is possible we will add multi-client support here after our initial release. |
| DirectMusic | No compatibility planned. Not part of our testing. |

Note that we are also investigating and experimenting with how to best incorporate the existing in-box Roland GS / General MIDI Synth into this architecture. It's likely we will handle it as an additional transport, but we need to test some of the MIDI file players today as many of them make assumptions about which synth index is the GS synth, so this compatibility may come after the initial release.
