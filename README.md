# Windows MIDI Services

This project is the next-generation MIDI API for Windows, including MIDI 1.0, MIDI CI, and MIDI 2.0. It includes enhancements, a new USB class driver, new transports, and a suite of essential tools. The project adds many enhancements and bug fixes to our MIDI 1.0 support, and importantly adds support for the latest revisions to MIDI CI and MIDI 2.0 as approved by The MIDI Association.

> The open source USB MIDI 2.0 driver has been generously donated by [AMEI](https://www.amei.or.jp/), the **Association of Musical Electronics Industry**, and developed by [AmeNote :tm:](https://www.AmeNote.com/) in partnership with Microsoft. Please see the file headers for any additional copyright notices. A huge thank you to AMEI and its member companies for making this happen!

**This is an official Microsoft project**. Although Microsoft is an active member of the MIDI Association, and Pete is the chair of the MIDI Association Executive Board, and other contributors are on standards boards, this project is not affiliated with the MIDI Association other than as a consumer of and contributor to the standards. Affiliation with AMEI is disclosed above.

Here's a high-level view of the prototype. Details subject to change.

![High-level view of the MIDI stack](img/high-level-view.png)

> **Join the Discussion!**
>
> Our official community server for this project is on Discord here: https://aka.ms/MidiDiscord
>
> Please keep bug and feature requests in the issues here, but other discussion, live streams, Q&A, and more can happen on Discord. Additionally, we know that not everyone who uses MIDI has access to GitHub, so we welcome additional suggestions, reports, etc. there for those members of the community.

## Component parts

You'll notice that this repo contains a number of related projects all grouped together as "Windows MIDI Services"

| Project | Description |
| --------------- | ----------------------------------- |
| USB MIDI 2.0 Driver | The new MIDI 1.0 and MIDI 2.0 class driver for Windows |
| MIDI Service | MIDI in Windows now uses a Windows Service, like audio does today. This enables us to do a lot more on behalf of the user. The MIDI service is what talks to the various transports and drivers directly. |
| MIDI Abstraction Layer | This is mostly part of the service. This provides the abstractions we need to be able to enable different types of transports, and also enable compatibility with the current MIDI 1.0 APIs |
| New MIDI API | The API is the interface into the service. To ensure that we can ship the API with Windows and still keep up with evolving MIDI standards, much of the API uses JSON payloads for parameters and configuration and is largely just a direct pipe. Our intent is for application developers to avoid using the API directly in most cases, but to instead use the SDK. |
| MIDI SDK | The SDK is shipped with individual applications. It provides strongly-typed entry points into and interpretations of API information. It also helps ensure applications name and treat MIDI entities in similar or identical ways. Additionally, the SDK can rev at the speed needed to keep up with updates to the MIDI specifications, without breaking compatibility with the operating system API. **We encourage all application developers to use the SDK rather than the API directly.** |
| MIDI Settings Tool | This is the first of the end-user-focused tools we are delivering with Windows MIDI Services. It is a GUI tool which helps the user manage the MIDI system, and also perform tests, provide information to product support teams, perform common tasks such as sending/receiving SysEx, and much more. |

### Transports

Here are the current plans for transports. In general, transports are implemented as plugins into the Windows Service. Some, like USB, require related drivers, but the majority do not.

| Transport | Description |
| --------------- | ----------------------------------- |
| USB | The USB transport code and driver. This will be delivered with the initial release |
| Virtual | This will be built-in |
| Network | The in-progress UDP-based Network specification for UWP. We have this prototyped, and will deliver shortly after the specification is finalized. UWP Endpoints from Network will show up just like any other transport in the API/SDK. This transport will not require any other third-party products. We are also using the Network protocol, alongside the USB protocol, to validate our abstraction layers and plugin approach.|
| BLE | BLE MIDI 1.0 is currently planned to be implemented clean-room to include in this repo as full open source, and also address bugs brought up from the broader developer and musician communities. This work has not yet started. |
| RTP | No current plans for implementing RTP MIDI 1.0. That may change in the future if the need is there. We would, of course, accept contributions here if someone wishes to make an RTP network plugin. In the meantime, the existing RTP solutions should continue to work as they do today. |

## Releases

We are in the process of setting up automated pipelines for building the component pieces of this project. This readme will be updated with build status information one that is in place.

**There are no end-user preview releases yet.**

**There are no developer preview releases yet.**

**There are no prototype releases yet.**

For maximum flexibility and compatibility, there are several release mechanisms for Windows MIDI Services components.

| Component | Preview Release Mechanism | Production Release Mechanism |
| --------------- | ------------------------------ | ------------------------------ |
| USB MIDI 2 Driver | (Unsigned) Driver install from Github | (Signed) In-box Windows with Windows Update updates |
| Windows Service | Github Install | In-box Windows with Windows Update updates |
| Initial Transports | With Windows Service | With Windows Service |
| New Transports | Github Install | TBD (Possibly through Store with link from Settings App) |
| API | Github Install | In-box Windows with Windows Update updates |
| SDK | Github Install | NuGet, Github Install, others TBD |
| Settings and Related Tools | Github Install | Microsoft Store on Windows, WinGet |
| Documentation and Samples | Github | Microsoft Learn |

**When will components be considered "production"?** The bar in Windows is very high for any big changes (especially anything which may break compatibility) to an API that has shipped in-box, so we want to ensure the API and service are truly production-ready before including them in-box. This will be based on stability/readiness/performance, with input from stakeholders including AMEI, and not a specific timeline.

## Philosophy - why is this Open Source?

We believe in the musician and music technology communities. We also know that the music creation tech community is a highly motivated and interested community, including those who are both musicians and developers who are working to move music technology forward.

We recognize that standards like MIDI 2.0 are living standards, with new transports, messages, and add-on capabilities added over time. In the past, we haven't been quick to adopt those in releases of Windows, despite a more agile approach to Windows development. We want to change that.

We proposed that the best way to maintain an API which keeps up with the evolving MIDI standards and offers early adopters in this community an opportunity to try them out, test, and contribute, is to open source everything that we can, and invite both internal teams and the community to contribute to MIDI. **We also love the idea of others learning from this source code to implement MIDI 2.0 on embedded devices and other operating systems as they need.**

In short, we believe this project is the best way to continue to develop MIDI on Windows in the long-term, and to give back to the whole MIDI developer community, regardless of which operating system they prefer.

> There are some pieces which require changes to core Windows for us to implement. For example, we have made some changes to our USB stack to enable detection of the new class driver without interfering with USB Audio 1.0 (USB Audio 1.0 and MIDI 1.0 are in the same driver, but in the new model, MIDI 1.0 and MIDI 2.0 are in the new driver, but Audio 1.0 stays in the old driver). We're also porting some driver model work, and may have some other shims which redirect MIDI 1.0 API calls to the new stack. Most or all of these pieces will not be open source because they are based on existing private Windows code. But, wherever we can be open, we are and will be. Our default approach for this project is open source.

## Contributing

See [CONTRIBUTING.md](get-started/midi-developers/contributors/)

## Security

See [SECURITY.md](SECURITY.md)

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos (AMEI, The MIDI Association, AmeNote, etc.) are subject to those third-party's policies.

## Relevant specifications

**TODO: Update these with the new specs as soon as they make it on to the site.**

These are the specifications which apply to this project today.

* [MIDI 2.0]( --- TODO ---)

Note that this project does not support the deprecated MIDI CI protocol negotiation and related mechanisms.

Older specifications

* [MIDI 2.0](https://www.midi.org/specifications/midi-2-0-specifications)
* [USB MIDI 2.0](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-2-0-2)
* [MIDI 1.0](https://www.midi.org/specifications/midi1-specifications)
* [USB MIDI 1.0](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-1-0-2)

## Learn more about MIDI 2.0

This project supports both MIDI 1.0 and MIDI 2.0 through an updated modern API. MIDI 1.0 has been around since 1983, but MIDI 2.0 is new. You can learn more about it through the links below.

[![NAMM 2022 MIDI 2.0 Update](https://img.youtube.com/vi/69hzeBFOPfo/mqdefault.jpg)](https://www.youtube.com/watch?v=69hzeBFOPfo)
[![Mike Kent on MIDI 2.0 Protocol Messages and UMP](https://img.youtube.com/vi/Kky1nlwz8-8/mqdefault.jpg)](https://www.youtube.com/watch?v=Kky1nlwz8-8)

* [Windows MIDI and Music Dev Blog](https://devblogs.microsoft.com/windows-music-dev/)
* [MIDI is about collaboration, not competition](https://midi.org/midi-articles/midi-is-about-collaboration-not-competition)
* [MIDI @ 40 and the new Windows MIDI Services](https://devblogs.microsoft.com/windows-music-dev/midi-40-and-the-new-windows-midi-services/)
