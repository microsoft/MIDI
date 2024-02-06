# Windows MIDI Services

This project is the next-generation MIDI API for Windows, including MIDI 1.0 and MIDI 2.0 (MIDI CI, and MIDI 2.0 UMP). It includes enhancements, a new USB class driver, new transports, and essential tools. The project adds many enhancements and bug fixes to our MIDI 1.0 support, and importantly adds support for the latest revisions to MIDI 2.0 as approved by The MIDI Association.

> The open source USB MIDI 2.0 driver has been generously donated by [AMEI](https://www.amei.or.jp/), the **Association of Musical Electronics Industry**, and developed by [AmeNote :tm:](https://www.AmeNote.com/) in partnership with Microsoft. Please see the file headers for any additional copyright notices. A huge thank you to AMEI and its member companies for making this happen!

**This is an official Microsoft project**. Although Microsoft is an active member of the MIDI Association, and Pete is the chair of the MIDI Association Executive Board, and other contributors are on standards boards, this project is not affiliated with the MIDI Association other than as a consumer of and contributor to the standards. Affiliation with AMEI is disclosed above.

Here's a high-level view of the end goal of this project. Details subject to change.

![High-level view of the MIDI stack](img/high-level-view.png)

## Key documentation

All documentation has been moved to our documentation pages [https://aka.ms/midi](https://aka.ms/midi)

> **Join the Discussion!**
>
> Our official community server for this project is on Discord here: https://aka.ms/MidiDiscord
>
> Please keep bug and feature requests in the issues here, but other discussion, live streams, Q&A, and more can happen on Discord. Additionally, we know that not everyone who uses MIDI has access to GitHub, so we welcome additional suggestions, reports, etc. there for those members of the community.

## Component parts

You'll notice that this repo contains a number of related components all grouped together as "Windows MIDI Services"

| Location | Component | Description |
|--------| --------------- | ----------------------------------- |
| src\api\Drivers\USBMIDI2\ | USB MIDI 2.0 Driver | The new MIDI 1.0 and MIDI 2.0 class driver for Windows |
| src\api\Service\ | MIDI Service | MIDI in Windows now uses a Windows Service, like audio does today. This enables us to do a lot more on behalf of the user. The MIDI service is what talks to the various transports and drivers directly. |
| src\api\Abstraction\ | Transport Plugins | These are plugins referred to as "Abstractions". These are COM components that run in the service and talk to the different devices (USB, network, more). |
| src\api\Transform\ | Message Processing Plugins | These are plugins referred to as "Transforms". Like transports, these are COM components which run in the service. They provide features such as outbound message scheduling, translating between MIDI 1.0 byte stream and MIDI UMP, Jitter Reduction Timestamp handling, and more. |
| src\api\Client\Midi2Client\ | WinRT MIDI API | The WinRT MIDI API `Windows.Devices.Midi2` is the interface into the service and provides the mechanism by which applications use MIDI on Windows. |
| src\user-tools\midi-console\ | MIDI MIDI Services Console | This is a richly featured console application for monitoring endpoints, sending messages, automating tasks, testing, and much more. Written in C# using .NET 8 |
| src\user-tools\midi-settings\ | MIDI Settings Tool | This is a GUI tool which helps the user manage the MIDI system, rename endpoints, and also perform tests, provide information to product support teams, perform common tasks such as sending/receiving SysEx, and much more. We will deliver this after the initial release of Windows MIDI Services |
| src\oob-setup\ | Out of Band Delivery Setup Package | A WiX MSI installer for github-delivered releases of Windows MIDI Services. Our internal build and delivery process has no dependency on this. |

### Transports

In general, transports are implemented as plugins into the Windows Service. Some, like USB, require related drivers, but the majority are user-mode code.

| Location | Transport | Description |
| -------- | --------------- | ----------------------------------- |
| src\api\Abstraction\KSAbstraction\ | USB (Kernel Streaming) | The USB transport code and driver. This will be delivered with the initial release |
| src\api\Abstraction\VirtualMidiAbstraction\ | Virtual / App-to-App MIDI | This is what applications can use to create their own virtual MIDI Endpoints and be treated as though they are an external device. |
| src\api\Abstraction\DiagnosticsAbstraction\ | Diagnostics Loopback | These two endpoints are for testing and debugging purposes. [Learn more.](get-started/midi-developers/app-developers/docs/README.md)   |
| src\api\Abstraction\NetworkMidiAbstraction\ | Network MIDI 2.0 | The in-progress UDP-based Network specification for UWP. We have this prototyped and will deliver our implementation after we ship v 1.0 of Windows MIDI Services. UWP Endpoints from Network will show up just like any other transport in the API/SDK. This transport will not require any other third-party products. |
| src\api\Abstraction\BleMidiAbstraction\ | BLE | BLE MIDI 1.0 is currently planned to be implemented clean-room to include in this repo as full open source, and also address bugs brought up from the broader developer and musician communities. TBD on when this will ship |
| na | RTP | No current plans for implementing RTP MIDI 1.0. That may change in the future if the need is there. We would, of course, accept contributions here if someone wishes to make an RTP network plugin. In the meantime, the existing RTP solutions should continue to work as they do today. |

Third-parties may also create transports for prototyping new MIDI standards, or for otherwise adding new connectivity options. Transports are COM objects which implement a specific set of interfaces, and are loaded based on values stored in the Windows MIDI Services section of the registry. No driver or kernel programming knowledge is required.

### Transforms

Here are the in development and/or planned message transforms

| Location | Purpose | Description |
| -------- | --------------- | ----------------------------------- |
| src\api\Transform\ByteStreamToUMP\ | Translation | Translates MIDI 1.0 byte stream messages to UMP for legacy APIs and drivers |
| src\api\Transform\UMPToByteStream\ | Translation | Translates message formats from UMP to Byte Stream |
| src\api\Transform\EndpointMetadataListenerTransform\ | Metadata Capture | Monitors in-coming stream messages and stores Endpoint and Function Block info into the device properties |
| src\api\Transform\JitterReductionGeneratorTransform\ | JR Timestamps | Generates outgoing JR Timestamps when the stream is configured to do so. |
| src\api\Transform\JitterReductionListenerTransform\ | JR Timestamps | Listens for incoming JR Timestamps when the stream is configured to do so.  Removes them from the stream. |
| src\api\Transform\SchedulerTransform\ | Outbound Scheduling | Schedules outgoing messages based on timestamp. |

We will develop additional transforms in the future for things like optionally translating Note On velocity 0 to a Note Off message (required for some Mackie devices, for example), remapping groups/channels, remapping notes, etc. Those will be available after the MIDI Settings app is available.

## Releases

We are currently releasing only for developers. The end-user releases will be in publicly available Windows builds or servicing updates when ready.

### Developer Releases

Please see the [releases section of this repo](https://github.com/microsoft/MIDI/releases).

### End-user Releases

**There are no end-user preview releases yet.**

**When will components be considered "production"?** The bar in Windows is very high for any big changes (especially anything which may break compatibility) to an API that has shipped in-box, so we want to ensure the API and service are truly production-ready before including them in-box. This will be based on stability/readiness/performance, with input from stakeholders including AMEI, and not necessarily a specific timeline (although we do need to support partner product launches, and we do have broad milestone dates).

> Although we do track some Windows-internal specific items inside Microsoft in Azure DevOps, [here's everything we're tracking in the open](https://github.com/orgs/microsoft/projects/339).

## Building MIDI 2.0 Products

First, if you are building a commercial MIDI 2.0 product, we strongly encourage you to [become a corporate member of the MIDI Association](https://aka.ms/midiorgjoin). Doing so will support MIDI development, provide access to the corporate member forums where most of the discussions happen, access to in-progress transports and updates, the ability to shape the future of MIDI, and if your product qualifies, the right to use the MIDI logo.

We encourage you to begin building your products using the information in this repo. If you run into any problems with your products integrating with the implementation here, please be sure to open an issue if you believe it is a bug or omission, or have a discussion about it on Discord. 

For planning purposes, we expect to have a first version of the full Windows MIDI Services stack out later this year (2024). We're likely to release for Windows 11 first, and then as we get the required USB stack changes into the latest support Windows 10, we'll provide it for Windows 10. 

[MIDI Association article on building MIDI 2.0 USB devices](https://midi.org/midi-articles/building-a-usb-midi-2-0-device-part-1)

## License

* See [LICENSE](LICENSE)
* For a statement of intent and some other FAQs about forking and more, see [FAQ-License-and-Intent.md](FAQ-License-and-Intent.md)

## Contributing

See [CONTRIBUTING.md](get-started/midi-developers/contributors/)

## Security

See [SECURITY.md](SECURITY.md)

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos (AMEI, The MIDI Association, AmeNote, etc.) are subject to those third-party's policies.

## Learn more about MIDI 2.0

This project supports both MIDI 1.0 and MIDI 2.0 through an updated modern API. MIDI 1.0 has been around since 1983, but MIDI 2.0 is new. You can learn more about it through the links below.

[![MIDI 2.0 at the NAMM Show 2024](https://img.youtube.com/vi/1AZmAvaeBjM/mqdefault.jpg)](https://www.youtube.com/watch?v=1AZmAvaeBjM)

Older videos:

[![ADC 2022 Apple, Google, and Microsoft Implementations of MIDI 2.0](https://img.youtube.com/vi/AVQeHsBZjxc/mqdefault.jpg)](https://www.youtube.com/watch?v=AVQeHsBZjxc)
[![NAMM 2022 MIDI 2.0 Update](https://img.youtube.com/vi/69hzeBFOPfo/mqdefault.jpg)](https://www.youtube.com/watch?v=69hzeBFOPfo) 
[![Mike Kent on MIDI 2.0 Protocol Messages and UMP](https://img.youtube.com/vi/Kky1nlwz8-8/mqdefault.jpg)](https://www.youtube.com/watch?v=Kky1nlwz8-8)

* [Windows MIDI and Music Dev Blog](https://devblogs.microsoft.com/windows-music-dev/)
* [MIDI is about collaboration, not competition](https://midi.org/midi-articles/midi-is-about-collaboration-not-competition)
* [MIDI @ 40 and the new Windows MIDI Services](https://devblogs.microsoft.com/windows-music-dev/midi-40-and-the-new-windows-midi-services/)
* [Join the MIDI Association](https://aka.ms/midiorgjoin/)
