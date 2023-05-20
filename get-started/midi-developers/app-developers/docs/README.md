# Documentation

API and other documentation which will eventually be used in either the Microsoft.com docs portal, or in a github site. Format is markdown. Work on the docs has not really started.

## Some basics

The API, SDK, tools, service, drivers, and more are being developed with some guidelines about how apps could/should interact with MIDI on Windows. In no specific order or organization, here's a high-level look at what we plan to do here. In most cases, the MIDI API provides raw, and not easily used information, but the MIDI SDK provides everything an application needs to use the API in a way compatible with the MIDI Association MIDI 2.0 Implementation Guide.

### Enumeration

MIDI 2.0 enumeration involves both physically connected devices discoverable through PnP, as well as in-protocol information.

#### Enumerate UMP Endpoints

Windows.Devices.Enumeration provides the classes required to enumerate MIDI devices, and to watch for add/removal of devices, as it. We're building upon that and use the SDK to interpret the enumerated information as well as provide additional information about the devices.

#### Get Group Block or Function Block information for a UMP Endpoint

Function blocks could be requested through in-protocol UMP messages. Our intent is that applications not do this themselves, but that they use an SDK call to request this information. That gives us flexibility in how we can provide this information for older devices, and also reduce the number of Function Block information request calls.

Group Block information is provided by the USB driver, and passed through the API to the SDK.

### Names and Formatting

MIDI 2.0 naming of entities is more complex than it was with MIDI 1.0. Names can be sourced from different places through both enumeration and in-protocol messages. We would like to keep names consistent across applications and Windows. To that end, the SDK includes code to provide formatted, and when appropriate, localized names for entities.

#### Format Function Block, Group Block, UMP Endpoint names

The SDK will provide properties with this information formatted per the implementation guide.

#### Format Group and Channel names

The SDK has calls to enumerate Groups and Channels using data provided by MIDI 2.0 and MIDI CI. The results are formatted as per the MIDI 2.0 implementation guide and with the correct number transition (0-15 indexes changed to 1-16 for display, Channel names listed when available, Function Block names included with the group, etc.)

### USB MIDI 2.0 UMP Endpoints

#### Support more than one UMP Endpoint on a MIDI 2.0 USB device

We recommend you use a separate MIDI interface for each UMP Endpoint exposed by a USB device. This makes it easier to correctly association group blocks and with the resulting UMP Endpoints. Our driver does not support multiple UMP Endpoints on a single interface.

#### Use both Function Blocks and Group Blocks

The MIDI Association implementation guide has information on using both function blocks and group blocks together. These need to be logically consistent and not overlap in incompatible ways. This is especially important for how macOS uses these entities, so we don't want to have them represented differently on the operating systems.
