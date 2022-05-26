# Windows MIDI Services

This project is the next-generation MIDI API for Windows, including MIDI 1.0, MIDI CI, and [MIDI 2.0](https://www.midi.org/specifications/midi-2-0-specifications)
features, enhancements, a new driver, and an ecosystem of essential tools.

> The open source driver has been generously donated by [AMEI](https://www.amei.or.jp/),
> the Association of Musical Electronics Industry, and developed by AmeNote. Please see
> the file headers for any additional copyright notices. A huge thank you to AMEI and its
> member companies for making this happen!

Building upon the work we did with WinRT MIDI, we're making some big enhancements and also
addressing areas where we could do better with that API. Like the existing WinRT API, this
API may be called from a variety of langauges, including [C++/WinRT](https://docs.microsoft.com/windows/uwp/cpp-and-winrt-apis/),
from desktop apps and more.

Although this API will coexist with the existing MIDI 1.0 WinMM API, for backwards-compatibility
reasons, the intent is to have everyone converted to this new API going forward and to eventually
deprecate the WinMM API, as it doesn't support many of the features in demand today.

## Philosophy

At Microsoft, we believe in the musician and music technology communities. We know that
standards like MIDI are living standards, with new transports and add-on capabilities
added over time. The best way to maintain an API which keeps up with the evolving standards
and offers early adopters an opportunity to try them out, test, and contribute, is to open
source everything that we can, and inviting both internal teams and the community to contribute
to MIDI as implemented on Windows. We also love the idea of others learning from this source
code to implement MIDI 2.0 on embedded devices and other operating systems as they need.

## Feature Set

This is a living and growing API. The first phase is to release a complete and working MIDI 1.0
and MIDI 2.0 system that includes all the necessary infrastructure for us to continue to build upon.

### Short-term project "1.0" features

* A new combined multi-client MIDI 1.0, MIDI CI, and [MIDI 2.0 USB class driver](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-2-0-2)
* A new combined multi-client MIDI 1.0, MIDI CI, and MIDI 2.0 API based on the Universal MIDI Packet (UMP)
accessible to, at a minimum, C++ and C# applications.
* Infrastructure to provide for future implementations of virtual MIDI, MIDI processing like the MIDI
Mapper used to provide, new transports and much more, all without requiring, in most cases, new driver
code.
* Additional API and enumeration information to allow for richer information about ports, as well
as for end-user aliasing (renaming without eliminating the original name) of ports
* Bluetooth MIDI 1.0 (currently supported in WinRT MIDI only)

The majority of this infrastructure development will be done by the core team, mentioned below. We
certainly encourage you to participate in testing and in pull requests as you have suggestions.

### Long-term project features

* Virtual / App-to-App MIDI
* New MIDI transport standards (Bluetooth, network, etc.) as they are adopted by the MIDI Association
* A full suite of MIDI utility, debugging, configuration, and diagnostic tools both visual and
command-line. Examples:
  * command-line tools for enumerating devices, and performing typical automatable tasks
  * A friendly user interface for managing MIDI devices and their configuration on the PC
  * A MIDI monitor
  * Sysex transfer and librarian tools
* MIDI processor plug-ins
  * Remap MIDI notes, velocity curves, and more
  * Reroute MIDI messages
  * Filter out certain MIDI messages for a specific port

For these long-term goals, we will be partnering with the community of developers to help ensure we
have the right tools and utilities to meet your support needs and everyone's customers' needs, all
available as part of a single standard installation.

## Releases

To keep the project agile, we are currently planning to distibute all of the released and
signed end-user components through the Microsoft Store on Windows. This may evolve over time
as the project stabilizes or as the developer and musician community provide feedback.

## Meet the Core Team for 1.0

This is the core development team for the initial release. There are others involved who come in and
out as needed, and many more contributors to this project including those playing non-development
roles.

| Team Member | Company | Role in this Project |
| ---------------|---------|--------------------------------|
| [Pete Brown](https://github.com/Psychlist1972) | Microsoft | Overall Project and OSS Lead |
| Jay DiFuria | Microsoft | Microsoft PM |
| Edward Sumanaseni | Microsoft | Developer Lead |
| [Gary Daniels](https://github.com/garydan42) | Microsoft | Developer |
| [Julia DZmura](https://github.com/judzmura) | Microsoft | Developer |
| Ben Israel | Yamaha | AMEI Project Lead |
| Mike Kent | AmeNote | Driver Developer |
| Michael Loh | AmeNote | Driver Developer |

## Contributing

This project welcomes contributions and suggestions. Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft
trademarks or logos is subject to and must follow 
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.

## Relevant specifications

[MIDI 2.0](https://www.midi.org/specifications/midi-2-0-specifications)
[USB MIDI 2.0](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-2-0-2)
[MIDI 1.0](https://www.midi.org/specifications/midi1-specifications)
[USB MIDI 1.0](https://www.midi.org/specifications/midi-transports-specifications/usb/usb-midi-1-0-2)