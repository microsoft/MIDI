# Windows MIDI Services

This project is the next-generation MIDI API for Windows, including MIDI 1.0, MIDI CI, and MIDI 2.0. It includes enhancements, a new USB class driver, new transports, and a suite of essential tools. The project adds many enhancements and bug fixes to our MIDI 1.0 support, and importantly adds support for the latest revisions to MIDI CI and MIDI 2.0 as approved by The MIDI Association.

> The open source USB MIDI 2.0 driver has been generously donated by [AMEI](https://www.amei.or.jp/), the **Association of Musical Electronics Industry**, and developed by [AmeNote :tm:](https://www.AmeNote.com/) in partnership with Microsoft. Please see the file headers for any additional copyright notices. A huge thank you to AMEI and its member companies for making this happen!

**This is an official Microsoft project**. Although Microsoft is an active member of the MIDI Association, and Pete is the chair of the MIDI Association Executive Board, and other contributors are on standards boards, this project is not affiliated with the MIDI Association other than as a consumer of and contributor to the standards. Affiliation with AMEI is disclosed above.

Here's a high-level view of the end goal. Details subject to change.

![High-level view of the MIDI stack](img/high-level-view.png)

> **Join the Discussion!**
>
> Our official community server for this project is on Discord here: https://aka.ms/MidiDiscord
>
> Please keep bug and feature requests in the issues here, but other discussion, live streams, Q&A, and more can happen on Discord. Additionally, we know that not everyone who uses MIDI has access to GitHub, so we welcome additional suggestions, reports, etc. there for those members of the community.

## API Backwards Compatibility

Our intention is for developers to begin adopting Windows MIDI Services in place of the older WinMM, WinRT, and (deprecated) DirectMusic APIs in their applications. All new MIDI features, transports, and more will be implemented in Windows MIDI Services. A select number of features, slightly more than their current baseline, will be available to WinMM and WinRT APIs through our backwards-compatibility shims and abstractions, but this is simply to ensure existing applications continue to function on systems using Windows MIDI Services. **Please note that we are not providing backwards compatibility to support DirectMusic MIDI APIs.**

The existing MIDI APIs on Windows talk almost directly to MIDI 1.0 drivers. In Windows MIDI Services, the architecture is built around a central Windows Service, much like our audio system today. It also uses a much faster IO mechanism for speaking to the USB driver vs what our MIDI 1.0 API uses today. This provides much more flexibility, including the potential for multi-client use, and good baseline speed with our new class driver. We are working on shims and abstractions which will allow some of the existing MIDI 1.0 APIs to talk to the service rather than directly to the driver.

Here is where we currently stand with planned backwards compatibility. Backwards compatibility for WinMM and WinRT APIs will be a post-1.0 feature, but shortly after that first release.

| API | What you should expect |
| --------------- | ----------------------------------- |
| Windows MIDI Services | This project. 100% of all supported features for MIDI 1.0 and MIDI 2.0, including multi-client. API/SDK uses UMP as its internal data format even for MIDI 1.0 devices. Transports and the service handle translation. |
| WinMM (Win32 API most apps use today) | Access to MIDI 1.0 and most MIDI 2.0 devices, at a MIDI 1.0 compatibility level only. It is possible we will add multi-client support here after our initial release. |
| WinRT (MIDI API Introduced with Windows 10) | Access to MIDI 1.0 and most MIDI 2.0 devices, at a MIDI 1.0 compatibility level only. It is possible we will add multi-client support here after our initial release. |
| DirectMusic | No compatibility planned. Not part of our testing. |

Note that we are also investigating and experimenting with how to best incorporate the existing in-box Roland GS / General MIDI Synth into this architecture. It's likely we will handle it as an additional transport, but we need to test some of the MIDI file players today as many of them make assumptions about which synth index is the GS synth, so this compatibility may come after the initial release.

> MIDI is used for many things from musical performances to [Black MIDI](https://en.wikipedia.org/wiki/Black_MIDI) to controlling lights and even quadcopters (something I did on stage once, even). For the initial releases, our primary user audience for Windows MIDI Services, and therefore our priority when deciding on features, is **musicians using typical musician workflows** to make music and perform, manage their devices, etc. Use cases outside of this are not necessarily in-scope for the first release, but may be certainly considered for subsequent releases. This is especially important as the MIDI community works to arrive at best practices around things like jitter reduction timestamps, which can generate a significant amount of traffic. We're not going to do anything specifically to prevent any other use of MIDI, but we ask folks interested in those other uses to both provide feedback to us through issues here or Discord so we know what you are looking for and we can keep it in mind, and to be patient while we work through the priorities.

## Component parts

You'll notice that this repo contains a number of related projects all grouped together as "Windows MIDI Services"

| Project | Description |
| --------------- | ----------------------------------- |
| USB MIDI 2.0 Driver | The new MIDI 1.0 and MIDI 2.0 class driver for Windows |
| MIDI Service | MIDI in Windows now uses a Windows Service, like audio does today. This enables us to do a lot more on behalf of the user. The MIDI service is what talks to the various transports and drivers directly. |
| MIDI Abstraction Layer | This is mostly part of the service. This provides the abstractions we need to be able to enable different types of transports, and also enable compatibility with the current MIDI 1.0 APIs |
| New MIDI API | The API is the interface into the service. To ensure that we can ship the API with Windows and still keep up with evolving MIDI standards, much of the API uses JSON payloads for parameters and configuration and is largely just a direct pipe to the service. Our intent is for application developers to avoid using the API directly in most cases, but to instead use the SDK. |
| MIDI App SDK | The SDK is shipped with individual applications. It provides strongly-typed entry points into and interpretations of API information. It also helps ensure applications name and treat MIDI entities in similar or identical ways. Additionally, the SDK can rev at the speed needed to keep up with updates to the MIDI specifications, without breaking compatibility with the operating system API. **We encourage all application developers to use the SDK rather than the API directly.** |
| MIDI Settings Tool | This is the first of the end-user-focused tools we are delivering with Windows MIDI Services. It is a GUI tool which helps the user manage the MIDI system, and also perform tests, provide information to product support teams, perform common tasks such as sending/receiving SysEx, and much more. |

### Transports

Here are the current plans for transports. In general, transports are implemented as plugins into the Windows Service. Some, like USB, require related drivers, but the majority are user-mode code.

| Transport | Description |
| --------------- | ----------------------------------- |
| USB | The USB transport code and driver. This will be delivered with the initial release |
| Virtual and Loopback | These will be early transport plugins |
| Network MIDI 2.0 | The in-progress UDP-based Network specification for UWP. We have this prototyped, and will deliver shortly after the specification is finalized. UWP Endpoints from Network will show up just like any other transport in the API/SDK. This transport will not require any other third-party products. We are also using the Network protocol, alongside the USB protocol, to validate our abstraction layers and plugin approach.|
| BLE | BLE MIDI 1.0 is currently planned to be implemented clean-room to include in this repo as full open source, and also address bugs brought up from the broader developer and musician communities. This work has not yet started. |
| RTP | No current plans for implementing RTP MIDI 1.0. That may change in the future if the need is there. We would, of course, accept contributions here if someone wishes to make an RTP network plugin. In the meantime, the existing RTP solutions should continue to work as they do today. |

## Releases

We are in the process of setting up automated pipelines for building (and eventually signing) the component pieces of this project. This readme will be updated with build status information one that is in place. **There are no usable builds, currently.**

### Developer Releases

Please see the [releases section of this repo](https://github.com/microsoft/MIDI/releases).

### End-user Releases

**There are no end-user preview releases yet.**

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
| Documentation and Samples | Github | Microsoft Learn / Docs |

**When will components be considered "production"?** The bar in Windows is very high for any big changes (especially anything which may break compatibility) to an API that has shipped in-box, so we want to ensure the API and service are truly production-ready before including them in-box. This will be based on stability/readiness/performance, with input from stakeholders including AMEI, and not necessarily a specific timeline (although we do need to support partner product launches, and we do have broad milestone dates).

> Although we do track some Windows-internal specific items inside Microsoft in Azure DevOps, [here's everything we're tracking in the open](https://github.com/orgs/microsoft/projects/339).

## Building MIDI 2.0 Products

First, if you are building a commercial MIDI 2.0 product, we strongly encourage you to [become a corporate member of the MIDI Association](https://aka.ms/midiorgjoin). Doing so will support MIDI development, provide access to the corporate member forums where most of the discussions happen, access to in-progress transports and updates, the ability to shape the future of MIDI, and if your product qualifies, the right to use the MIDI logo.

We encourage you to begin building your products using the information in this repo. If you run into any problems with your products integrating with the implementation here, please be sure to open an issue if you believe it is a bug or omission, or have a discussion about it on Discord. 

For planning purposes, we expect to have a first version of the full MIDI 2.0 stack out near the end of this year. We'll update everyone as that gets more solid.

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

## Relevant specifications

These are the updated MIDI 2.0 (June 2023) specifications which apply to this project today.

* [MIDI 2.0](https://midi.org/specifications)

Note that this project does not support the deprecated MIDI CI protocol negotiation and related mechanisms.

Older specifications

* [MIDI 2.0](https://www.midi.org/specifications/midi-2-0-specifications)
* [USB MIDI 2.0](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-2-0-2)
* [MIDI 1.0](https://www.midi.org/specifications/midi1-specifications)
* [USB MIDI 1.0](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-1-0-2)

## Learn more about MIDI 2.0

This project supports both MIDI 1.0 and MIDI 2.0 through an updated modern API. MIDI 1.0 has been around since 1983, but MIDI 2.0 is new. You can learn more about it through the links below.

[![ADC 2022 Apple, Google, and Microsoft Implementations of MIDI 2.0](https://img.youtube.com/vi/AVQeHsBZjxc/mqdefault.jpg)](https://www.youtube.com/watch?v=AVQeHsBZjxc)
[![NAMM 2022 MIDI 2.0 Update](https://img.youtube.com/vi/69hzeBFOPfo/mqdefault.jpg)](https://www.youtube.com/watch?v=69hzeBFOPfo) 
[![Mike Kent on MIDI 2.0 Protocol Messages and UMP](https://img.youtube.com/vi/Kky1nlwz8-8/mqdefault.jpg)](https://www.youtube.com/watch?v=Kky1nlwz8-8)

* [Windows MIDI and Music Dev Blog](https://devblogs.microsoft.com/windows-music-dev/)
* [MIDI is about collaboration, not competition](https://midi.org/midi-articles/midi-is-about-collaboration-not-competition)
* [MIDI @ 40 and the new Windows MIDI Services](https://devblogs.microsoft.com/windows-music-dev/midi-40-and-the-new-windows-midi-services/)
* [Join the MIDI Association](https://aka.ms/midiorgjoin/)
