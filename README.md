# Windows MIDI Services

This project is the next-generation MIDI API for Windows, including MIDI 1.0, MIDI CI, and [MIDI 2.0](https://www.midi.org/specifications/midi-2-0-specifications) features, enhancements, a new driver, and an ecosystem of essential tools. It builds upon the work we did for WinRT MIDI, adds many enhancements and bug fixes to our MIDI 1.0 support, and importantly adds support for MIDI CI and MIDI 2.0.

This is an official Microsoft project.

> The open source driver has been generously donated by [AMEI](https://www.amei.or.jp/),
> the **Association of Musical Electronics Industry, and developed by AmeNote**. Please see
> the file headers for any additional copyright notices. A huge thank you to AMEI and its
> member companies for making this happen!

Building upon the work we did with WinRT MIDI, we're making some big enhancements and also
addressing areas where we could do better with that API. Like the existing WinRT API, this
API may be called from a variety of langauges, including [C++/WinRT](https://docs.microsoft.com/windows/uwp/cpp-and-winrt-apis/), from desktop apps and more.

> **Join the Discussion!**
> 
> Our official community server for this project is on Discord here: 
> https://aka.ms/MidiDiscord
> 
> Please keep bug and feature requests in the issues here, but other discussion, live streams, 
> Q&A, and more can happen on Discord. Additionally, we know that not everyone who uses MIDI
> has access to GitHub, so we welcome additional suggestions, reports, etc. there for those
> members of the community.

## Philosophy - why is this Open Source?

We believe in the musician and music technology communities. We also know that
the music creation tech community is a highly motivated and interested community, including
those who are both musicians and developers who are working to move music technology forward.

We recognize that standards like MIDI are living standards, with new transports and add-on
capabilities added over time and that in the past, we haven't been quick to adopt those in
released of Windows, and we want to change that.

We propose that the best way to maintain an API which keeps up with the evolving standards
and offers early adopters in this community an opportunity to try them out, test, and contribute,
is to open source everything that we can, and inviting both internal teams and the community to
contribute to MIDI as implemented on Windows. We also love the idea of others learning from this
source code to implement MIDI 2.0 on embedded devices and other operating systems as they need.

In short, we believe this project is the best way to continue to develop MIDI on Windows, and to
give back to the whole MIDI developer community, regardless of which operating system they
prefer.

## Repo Structure

The repo contains many different projects and stand-alone files which make up Windows MIDI Services.

| Location             | Description |
| ---------------------|----------------------------------------------------------|
| /                    | Project root, including main README |
| /get-started         | Root for all samples and documentation |
| /get-started/docs    | Developer and user documentation |
| /get-started/samples | Developer samples for using the API |
| /src                 | Main source (non-documentation/sample) root |
| /src/api             | MIDI API for Windows |
| /src/plug-ins        | Processing, transport, and other plug-ins |
| /src/shared          | Non-developer-facing shared components used by tools, API, etc. e.g logging, config |
| /src/tools           | End-user tools like the settings app and command-line interface |
| /src/ui              | Reusable WinUI3-compatible UI components related to the MIDI API |
| /src/usb-driver      | Windows USB MIDI 2.0 and 1.0 Class driver source |

Note that any internal changes made to the OS to support the driver or MIDI API are not contained in this repo.

## Feature Set

This is a living and growing API. The first phase is to release a complete and working MIDI 1.0 and MIDI 2.0 system that includes all the necessary infrastructure for us to continue to build upon.

> Some big features are called out in this readme. Beyond that, specific detailed features may be found in
> the issues list by using the following labels:
>
> Proposed Features
> https://github.com/microsoft/MIDI/labels/feature%20proposal
> 
> Approved Features
> https://github.com/microsoft/MIDI/labels/feature
> 
> Known Tracked Bugs
> https://github.com/microsoft/MIDI/labels/bug

### Short-term project big "1.0" features

Here are some selected big rocks intended for the first release. Our top priority for the first release is a fully functional MIDI 2.0, 1.0, and MIDI-CI driver and API, usable with third-party applications and devices.

* A new combined multi-client MIDI 1.0, MIDI CI, and [MIDI 2.0 USB class driver](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-2-0-2)
* A new combined multi-client MIDI 1.0, MIDI CI, and MIDI 2.0 API based on the Universal MIDI Packet (UMP)
accessible to, at a minimum, C++ and C# applications.
* Infrastructure to provide for future implementations of virtual MIDI, MIDI processing like the MIDI Mapper used to provide, new transports and much more, all without requiring, in most cases, new driver code.
* Infrastructure for a JSON-based configuration system for MIDI system profiles.
* Additional API and enumeration information
  * End-user renaming / aliasing of devices / endpoints while also keeping the driver-supplied name accessible
  * More detailed information about devices (driver details, manufacturer info, etc.)
  * Transport information so apps know how the device is connected (USB, BLE, Network, etc.)
  * more
* Bluetooth MIDI 1.0 (currently supported in WinRT MIDI or through third-party drivers)

The majority of this infrastructure development will be done by the core team, mentioned below. We
certainly encourage you to participate in testing and in pull requests as you have suggestions.

### Long-term project features

Here are some other features that are more long-term. Some may make it into the first release, but we will not block on them for 1.0. This is what we intend to do, but is not a commitment at this point.

* Virtual / App-to-App MIDI
* New MIDI 2 transport standards (Bluetooth, network, etc.) as they are adopted by the MIDI Association
* A full suite of MIDI utility, debugging, configuration, and diagnostic tools both visual and
command-line. Including:
  * A friendly user interface for managing MIDI devices and their configuration on the PC
  * command-line tools for enumerating devices, and performing typical automatable tasks
  * A MIDI monitor
  * Sysex transfer and librarian tools
  * Possible musician performance-focused tools like patch switching, controllers, etc.
* MIDI message processor plug-ins
  * Remap MIDI notes, velocity curves, and more
  * Reroute MIDI messages
  * Filter out certain MIDI messages for a specific endpoint
  * more as proposed and/or contributed by the community

For these long-term goals, we will be partnering with the community of developers to help ensure we have the right tools and utilities to meet your support needs and everyone's customers' needs, all available as part of a single standard installation.

## Releases

To keep the project agile, we are currently planning to distibute all of the released and signed end-user components, as much as possible, through the Microsoft Store on Windows. This may evolve over time as the project stabilizes or as the developer and musician community provide feedback. Additionally, some components may need to be distributed through Windows Update or other common mechanisms. TBD as we get into releases.

**There are no end-user releases yet. We expect to start seeing those as we head into 2023.**

## Meet the Core Team for Release 1.0

This is the core development team for the initial release. There are others involved who come in and out as needed, and many more contributors to this project including those playing non-development roles.

| Team Member | Company | Role in this Project |
| ---------------|---------|--------------------------------|
| [Pete Brown](https://github.com/Psychlist1972) | Microsoft | Overall Project and OSS Lead, Tools & API Engineer, MIDI Association Executive Board member |
| Jay DiFuria | Microsoft | Microsoft API PM |
| Edward Sumanaseni | Microsoft | API Engineeringr Lead |
| [Gary Daniels](https://github.com/garydan42) | Microsoft | API Engineer  |
| [Julia DZmura](https://github.com/judzmura) | Microsoft | API Engineer |
| Ben Israel | Yamaha | AMEI Project Lead |
| [Mike Kent](AmeNote-MikeKent) | AmeNote | Driver Developer |
| [Michael Loh](AmeNote-Michael) | AmeNote | Driver Developer |
| [Andrew Mee](starfishmod) | | MIDI Consultant |

## Device, Programming Language, and API Compatibility

See See [README-Compatibility.md](README-Compatibility.md)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md)

## Security

See [SECURITY.md](SECURITY.md)

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos are subject to those third-party's policies.

## Relevant specifications

These are the specifications which apply to this project today.

* [MIDI 2.0](https://www.midi.org/specifications/midi-2-0-specifications)
* [USB MIDI 2.0](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-2-0-2)
* [MIDI 1.0](https://www.midi.org/specifications/midi1-specifications)
* [USB MIDI 1.0](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-1-0-2)

## Learn more about MIDI 2.0

[![NAMM 2022 MIDI 2.0 Update](https://img.youtube.com/vi/69hzeBFOPfo/mqdefault.jpg)](https://www.youtube.com/watch?v=69hzeBFOPfo)
[![Mike Kent on MIDI 2.0 Protocol Messages and UMP](https://img.youtube.com/vi/Kky1nlwz8-8/mqdefault.jpg)](https://www.youtube.com/watch?v=Kky1nlwz8-8)
