---
layout: kb
title: Map MIDI 1 port names to new endpoints
audience: developers
description: How to to map stored MIDI WinMM port names to current endpoints
---

# WinMM Naming

WinMM had a specific approach to naming MIDI 1.0 ports which resulted in names like `MIDIOUT2 (Some Device)`. That name worked well for decades, but was not what manufacturers and customers have asked for. So in Windows MIDI Services, we have introduced modern port naming, which uses the devices `iJack` name when available.

However, for decades, applications have had to store the MIDI 1.0 WinMM port names in their project and other files to be able to pull up the correct port when next launched. As a result, our default MIDI 1.0 port names in Windows MIDI Services are the WinMM-compatible names, complete with the naming bugs inherent in that approach (for example: devices with the same USB Vendor and Product Id, but different names, will result in both devices sharing one of the names).

We want to move away from these old-style names, but recognize there's a long (and perhaps never-ending) transition period. Therefore, we've included ways to get to the old-style names programmatically, even when the user wants to see only new-style names.

## The user is always in control

In Windows MIDI Services, the user is always in control. The user can set options for
- Global MIDI naming approach (WinMM compatible or New-Style)
- Per-Endpoint (not port) naming approach (Use the global setting, use WinMM compatible, or use New-Style)
- Custom names for any MIDI 1.0 port

As a result, the names may not match what you have stored in your files. **Apps shall not force the user to see one style of name or the other.** Always display the name that Windows MIDI Services has provided as the `Name` for any given entity. It is acceptable to provide a way to allow the user, with an additional click or in a properties window, to see old-style names if it benefits them and your app's flow. However, when displaying the name in tracks, always show the name the system has provided. Do not offer options in your apps to pick between display names, as this will add confusion. The only way a customer should change any Endpoint, Port, or other name is through the MIDI Settings app.

The Per-Endpoint port naming approach is available through the `Midi1PortNamingApproach` property of the `MidiEndpointDeviceInformation` type.

```cpp
namespace Microsoft.Windows.Devices.Midi2
{
    [contract(MidiEnumerationApiContract, 1)]
    enum Midi1PortNamingApproach
    {
        Default = 0,
        UseClassicCompatible = 1,
        UseNewStyle = 2,
    };
}
```

If you are not mapping to old names, there's nothing you need to do here. Windows MIDI Services will choose the user-preferred names when creating the MIDI 1.0 ports, and will also use the user-preferred name for the UMP Endpoint.

> Note: You may wonder about the Group Terminal Blocks and how they play into this. For an application which has adopted the new SDK, we prefer you show the endpoint's name (which takes into account user preferences) and then the Function Block names and Group Numbers if function blocks are available. Function Block Names come from the device, which can sometimes provide ways to change the name on-device. If no function blocks are available (for example, with a MIDI 1.0 device), the group terminal block names and group numbers. The Group Terminal Block names use new-style naming. If you need to map to old-style naming, use the group information and look up the value in the name table.
> For MIDI 1.0 devices, the Group Terminal Block is 1:1 with a MIDI 1.0 port.
> For MIDI 2.0 devices, per-spec, a Function Block can span multiple groups and directions, and a group can appear in multiple function blocks. The ultimate address is always the group. So you can end up with names like `Some Endpoint Group 5 (Synth, Sequencer)` if you follow recommended naming conventions for MIDI 2.0.
> USB MIDI 2.0 devices will have static Group Terminal Blocks and *may* also have static or dynamic function blocks, discovered in-protocol after the device has been plugged in, and updated at any point in time afterwards. Function blocks, when available, take precedence
> We may offer a way for users to rename Group Terminal Blocks and Function Blocks in the future. Do not store these names in your own files.

## Find original names

Each Windows MIDI Services Endpoint has a name table associated with it, which contains a vector of the following information:

```cpp
namespace Microsoft.Windows.Devices.Midi2
{
    // this needs to be kept in sync with Midi1PortNameEntry in service

    [contract(MidiEnumerationApiContract, 1)]
    struct Midi1PortNameTableEntry
    {
        UInt8 GroupIndex;               // can't use MidiGroup here unless we're going to make a runtimeclass of this
        Midi1PortFlow Flow;

        String CustomName;              // user-supplied name. Blank if not provided.
        String LegacyCompatibleName;    // WinMM MIDI 1.0 compatible port name
        String NewStyleName;            // New-style port name
    };
}
```
The name table is available from the MidiEndpointDeviceInformation type via the `GetNameTable()` function.

The table is re-generated whenever the device is discovered by the service. (So when it's plugged in, or when the service is restarted). 

If you have a WinMM MIDI Output port name, and you want to look it up in the table, you can look for `Flow == Midi1PortFlow::MidiMessageDestination` and `LegacyCompatibleName == <your stored WinMM name>`. Of course, you would need to do this for each of the enumerated MIDI Endpoints. But this is a very fast call, using only stored property information that is already available in memory.

Note: the fact that a name table entry exists does not necessarily equate to the MIDI 1.0 port being active or available. You will need to handle cases where no port with that specific name exists. In addition, due to the nature of MIDI 1.0 port names, you have to be able to handle instances where more than one match exists, especially in cases where more than one of the same make/model of device is connected.

## Ways to find the associated ports

Windows MIDI Services has a parent-child relationship with WinMM and WinRT MIDI 1.0 ports.

    `Device > Windows MIDI Services Endpoint > MIDI 1.0 port`

Each Windows MIDI Services Endpoint and each MIDI 1.0 port is created as a Software Device in Windows. The MIDI 1.0 port's software device information includes a property which points back up to the parent Windows MIDI Services Endpoint.

Because of these relationships, another way to find the associated MIDI 1.0 ports for an endpoint is to use one of these functions:

```cpp

// creates using a WinRT MIDI 1.0 device id. This is a relatively fast lookup of properties O(1)
// Does not use or modify cached values
static MidiEndpointDeviceInformation CreateFromAssociatedMidi1PortDeviceId(String deviceId);

// creates using a WinMM MIDI 1.0 port number. This requires looking up information in all device entries
// until a match has been found. As a result, it is O(n) where n is the number of MIDI 1.0 ports
// Does not use or modify cached values
static MidiEndpointDeviceInformation CreateFromAssociatedMidi1PortNumber(UInt32 portNumber, Midi1PortFlow portFlow);

// returns the parent Windows MIDI Services Endpoint Device Id which is associated with the MIDI 1.0 child port
// same complexity as CreateFromAssociatedMidi1PortNumber. Does not use or modify cached values
static String FindEndpointDeviceIdForAssociatedMidi1PortNumber(UInt32 portNumber, Midi1PortFlow portFlow);

// also O(n) complexity, but slightly faster because the generated query can use the ItemNameDisplay property.
// Does not use or modify cached values
static IVectorView<MidiEndpointDeviceInformation> FindAllForAssociatedMidi1PortName(String portName, Midi1PortFlow portFlow);
static IVectorView<String> FindAllEndpointDeviceIdsForAssociatedMidi1PortName(String portName, Midi1PortFlow portFlow);

// these are for finding the MIDI 1.0 (WinMM and WinRT 1.0) ports which were created from this UMP endpoint
// optionally uses cached values (default is the use the cache if available. Cache is generated on first call.)
// All of these functions are O(n), but using cached values significanly speeds up lookup time.
IVectorView<MidiEndpointAssociatedPortDeviceInformation> FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow portFlow);
IVectorView<MidiEndpointAssociatedPortDeviceInformation> FindAllAssociatedMidi1PortsForThisEndpoint(Midi1PortFlow portFlow, Boolean useCachedPortInformationIfAvailable);

MidiEndpointAssociatedPortDeviceInformation FindAssociatedMidi1PortForGroupForThisEndpoint(MidiGroup group, Midi1PortFlow portFlow);
MidiEndpointAssociatedPortDeviceInformation FindAssociatedMidi1PortForGroupForThisEndpoint(MidiGroup group, Midi1PortFlow portFlow, Boolean useCachedPortInformationIfAvailable);
```
Unlike the name table, these are all operating upon the domain of enumerated/created MIDI 1.0 ports available in the system at the time the call is made. The downside is that these calls are more expensive to make, and so shouldn't be called in any timing-critical code. The custom properties we include in endpoints cannot be used in an AQS query, and so we have to create an instance of each device, check its properties, and then move on.

However, they also cache the information by default, making the lookup much faster after the first call, because the devices do not need to be created and then the property queried. There's no automatic cache maintenance or expiration, so the information can become stale when devices are added or removed. Because device watchers are expensive to maintain in this context, it's up to the caller to decide, based on their own global-scope device watcher events, whether or not to use cached information.

The lookup cache is local to the `MidiEndpointDeviceInformation` instance. We may use the COM static store in the future to make the cache available to all instances, which could speed things up.

The functions are described in the [`MidiEndpointDeviceInformation` documentation](https://microsoft.github.io/MIDI/sdk-reference/MidiEndpointDeviceInformation/).
