---
title: Windows Midi Services
layout: home
has_children: true
---
# Windows MIDI Services

> [Source repo and developer releases on GitHub](https://aka.ms/midirepo)

> [Discord Server for discussion about this project](https://aka.ms/mididiscord)

## Key Features

* **Multi-client by default**. Unless an endpoint is configured to not allow shared connections, or there is some issue around multi-client in a third-party driver, any endpoint (including MIDI 1.0 devices) can be used by multiple applications at the same time. So far, in our testing, we haven't found any USB devices or drivers which cannot be multi-client.
* **Faster**. In our testing, we've found that the new infrastructure is much faster at sending and receiving messages compared to the older API, even with plugins configured in the service. There are no built-in speed caps or throttling in Windows MIDI Services, even for older USB MIDI 1.0 devices. The driver is not limited USB full-speed, and supports USB 3.x speeds.
* **Lower Jitter**. Along with higher speed comes lower jitter. This will vary by transport type (USB vs Network vs Virtual), and the device Windows is talking to, but the jitter is in the low microsecond range even without any compensation.
* **More Deterministic**. Speaking of latency compensation, the new API enables timestamp-based message scheduling for outbound messages, and also will soon support Jitter Reduction timestamps for MIDI 2.0 devices which can use them.
* **Extensible**. The service has been designed to be extensible by Microsoft and third-parties. New types of transports can be added at any time, including during prototyping of a new transport specification. (We're working on Network MIDI 2.0, Bluetooth MIDI 1.0 and considering RTP, all using this model.) Similarly, message processing plugins can also be developed by Microsoft or third-parties and used for production and/or prototyping. **No kernel driver experience required in most cases.**
* **App-to-App and Virtual MIDI**. Windows MIDI Services includes virtual / app-to-app MIDI 2.0 to enable lightning fast communication between apps on the PC. We're also investigating flexible routing between any MIDI endpoints as a future feature.
* **Better tools**. We supply the `midi.exe` Windows MIDI Services Console for developers and power users, or anyone comfortable with the command line. You can use it to monitor endpoints, send and receive messages, send/capture SysEx data and much more. We'll deliver the MIDI Settings GUI app after our initial release. That app enables renaming devices, configuring your MIDI setup, testing, and more.
* **UMP-Centric**. The new API fully embraces MIDI 2.0 and the Universal MIDI Packet format and handles all required translation in the service and driver. This makes the app model simple while ensuring all your existing devices continue to work.
* **Open Source**. The source code is open and available to everyone under a permissive license. Not sure how something works? Want to create a transport but aren't sure how we did it? Want to investigate a bug or contribute a feature? The code is there for you to explore.

Note: Additionally MIDI CI functionality, which does not technically require OS support, will be coming after version 1.0. We intend to add helpers for profiles, property exchange, MUID tracking, and more. In the meantime, applications can send and receive MIDI CI messages without anything in their way, using custom code or third-party libraries. MIDI CI is just MIDI 1.0-compatible SysEx.

## API Backwards Compatibility

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

## Resources


### Developer Materials

* [Get started examples](https://github.com/microsoft/MIDI/blob/main/samples/README.md)
* [API Documentation](developer-docs/)
* [Samples](../samples/)
* [Programming Languages FAQ](developer-docs/faq-programming-languages.md)

### Windows MIDI Services Console app

* [Console overview](midi-console.md)

### Advanced Materials

* [JSON Configuration File](config-json.md)

### Relevant specifications

These are the updated MIDI 2.0 specifications which apply to this project today.

* [MIDI 2.0 UMP Specifications](https://midi.org/specifications)

