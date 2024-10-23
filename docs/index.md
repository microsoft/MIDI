---
title: Overview
layout: home
has_children: false
---
# Windows MIDI Services

**NOTE: These docs are currently a work-in-progress**

The best place for questions and to stay up to date is on our Discord server. The best place to report bugs, as a developer, is the GitHub repo.

Windows MIDI Services is an open source project, which has been developed with input and feedback from the community. 

> [Source repo and developer releases on GitHub](https://aka.ms/midirepo)

> [Discord Server for discussion about this project](https://aka.ms/mididiscord)

## Get started

While we're in developer preview, get started by downloading and installing the latest release from GitHub here:

- [Latest Developer Releases](https://github.com/microsoft/MIDI/releases)

## NAMM Show 2024 Presentation

[![Pete's Windows MIDI Services Presentation at NAMM Show 2024](https://img.youtube.com/vi/-pe29zIVUCA/mqdefault.jpg)](https://www.youtube.com/watch?v=-pe29zIVUCA)

## Key Features

* **Multi-client by default**. Unless an endpoint is configured to not allow shared connections, or there is some issue around multi-client in a third-party driver, any endpoint (including MIDI 1.0 devices) can be used by multiple applications at the same time. So far, in our testing, we haven't found any USB devices or drivers which cannot be multi-client.
* **Faster**. In our testing, we've found that the new infrastructure is much faster at sending and receiving messages compared to the older API, even with plugins configured in the service. There are no built-in speed caps or throttling in Windows MIDI Services, even for older USB MIDI 1.0 devices. The driver is not limited USB full-speed, and supports USB 3.x speeds.
* **Lower Jitter**. Along with higher speed comes lower jitter. This will vary by transport type (USB vs Network vs Virtual), and the device Windows is talking to, but the jitter is in the low microsecond range even without any compensation.
* **More Deterministic**. Speaking of latency compensation, the new API enables timestamp-based message scheduling for outbound messages for any apps using the new API. In addition, incoming messages are tagged with a timestamp when received by the service.
* **Extensive Endpoint Metadata** We've built upon `Windows.Devices.Enumeration` with our own MIDI-specific device class, which provides much more information about a device. Not only do we have user-provided metadata (name, description, images, settings), but also more device-provided information. This is also where you have access to MIDI 2.0 device features like Function Blocks, Group Terminal Blocks, Device Identity, and much more.
* **Device Connect/Disconnect Notification** No restarting your apps to detect the presence of a new device. Whenever an endpoint is created, updated, or removed, the app receives an event with all the required details. Additionally, apps can opt-in to automatic reconnect to any device they are communicating with, should it become disconnected during a session.
* **Extensible**. The service has been designed to be extensible by Microsoft and third-parties. New types of transports can be added at any time, including during prototyping of a new transport specification. (We're working on Network MIDI 2.0, Bluetooth MIDI 1.0 and considering RTP, all using this model.) Similarly, message processing plugins can also be developed by Microsoft or third-parties and used for production and/or prototyping. **No kernel driver experience required in most cases.**
* **Includes app-to-App and Virtual Device MIDI**. Windows MIDI Services includes virtual / app-to-app MIDI 2.0 to enable lightning fast communication between apps on the PC. We're also investigating flexible routing between any MIDI endpoints as a future feature.
* **Better tools**. We supply the `midi.exe` Windows MIDI Services Console for developers and power users, or anyone comfortable with the command line. You can use it to monitor endpoints, send and receive messages, send/capture SysEx data and much more. We'll deliver the MIDI Settings GUI app after our initial release. That app enables renaming devices, configuring your MIDI setup, testing, and more.
* **UMP-Centric**. The new API fully embraces MIDI 2.0 and the Universal MIDI Packet format and handles all required translation in the service and driver. This makes the app model simple while ensuring all your existing devices continue to work. Messages to and from any endpoint, whether it is MIDI 1.0 or MIDI 2.0 data format, are transparently translated in the service, and presented as UMP through the API.
* **Open Source**. The source code is open and available to everyone under a permissive license. Not sure how something works? Want to create a transport but aren't sure how we did it? Want to investigate a bug or contribute a feature? The code is there for you to explore.
* **Supports your Existing Devices**. Windows MIDI Services supports the MIDI 1.0 devices you own today, including those with vendor-supplied kernel streaming drivers, as well as class-compliant MIDI 1.0 and MIDI 2.0 devices. The experience will be better/faster if they use the new class driver, but we recognize that is not always possible or desirable with some existing devices.
* **Supports your Existing Apps**. When Windows MIDI Services ships in-box in Windows, the existing WinMM and WinRT MIDI 1.0 APIs will be repointed to the new Windows Service. This will provide a subset of the new features, including multi-client. Those apps will not have access to the user metadata, auto-reconnect, device connect/disconnect detection, rich device details, MIDI 2.0 messages, etc. but will continue to work as today, with the benefit of multi-client (more than one app using the same port) support.

## Release Plans

We'll soon have Windows MIDI Services in Windows Insider releases. The first few releases will be opt-in for people who request access on our Discord server, as they are not complete and they *will* break some current MIDI applications. After that, we'll open it up more broadly.

After that, our release plan for the service, plugins, and API is to release when our customers, partners, and internal reviews are all happy with the results, especially backwards compatibility. This is currently expected to happen in the first half of CY 2025.

## Developer Samples

* [Developer Samples in the repo](https://github.com/microsoft/MIDI/tree/main/samples)

## Additional Resources

These are the updated MIDI 2.0 specifications which apply to this project today.

* [MIDI 2.0 UMP (and other) Specifications](https://midi.org/specs)
