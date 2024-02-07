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
* **Includes app-to-App and Virtual MIDI**. Windows MIDI Services includes virtual / app-to-app MIDI 2.0 to enable lightning fast communication between apps on the PC. We're also investigating flexible routing between any MIDI endpoints as a future feature.
* **Better tools**. We supply the `midi.exe` Windows MIDI Services Console for developers and power users, or anyone comfortable with the command line. You can use it to monitor endpoints, send and receive messages, send/capture SysEx data and much more. We'll deliver the MIDI Settings GUI app after our initial release. That app enables renaming devices, configuring your MIDI setup, testing, and more.
* **UMP-Centric**. The new API fully embraces MIDI 2.0 and the Universal MIDI Packet format and handles all required translation in the service and driver. This makes the app model simple while ensuring all your existing devices continue to work.
* **Open Source**. The source code is open and available to everyone under a permissive license. Not sure how something works? Want to create a transport but aren't sure how we did it? Want to investigate a bug or contribute a feature? The code is there for you to explore.

Note: Additionally MIDI CI functionality, which does not technically require OS support, will be coming after version 1.0. We intend to add helpers for profiles, property exchange, MUID tracking, and more. In the meantime, applications can send and receive MIDI CI messages without anything in their way, using custom code or third-party libraries. MIDI CI is just MIDI 1.0-compatible SysEx.

## Developer Samples

* [Developer Samples in the repo](../samples/)

## Additional Resources

These are the updated MIDI 2.0 specifications which apply to this project today.

* [MIDI 2.0 UMP Specifications](https://midi.org/specifications)

