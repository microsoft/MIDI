---
layout: kb
title: What are the Valid Persistent Endpoint Identifiers?
audience: developers
description: In old APIs like WinMM, the port name was the only identifier available. There are many more options now. This document provides guidance on what you can and cannot use.
---

> <h4>IMPORTANT NOTE</h4>
> While in development, the actual identifiers may change. Once we release officially to production Windows, the identifiers will be consistent as described below.

In the WinMM API today, the port name is often used as a unique identifier, stored in files, and otherwise used in comparisons when checking to see if the required MIDI devices are connected to the PC. They were never intended to be used this way, but because the Port numbers would change based upon, among other things, the order in which devices are plugged in, there were no other reasonable options.

We attempted to make the port numbers more "sticky" by keeping them static across device insert/removal events. However, the resulting sparse array of MIDI Ports broke around a third of the WinMM applications (they were not checking the result of the call to open a port, or worse, displayed an error dialog for each failed open). As a result, we had to scrap that approach and return to keeping that array fully populated, and the port numbers fluid.

For Windows MIDI Services, we provide additional information that a developer can use to identify an Endpoint. This information is all returned as properties of the `MidiEndpointDeviceInformation` class. The information below explains what properties can be relied upon and when/how.

> **Endpoint names** in Windows MIDI Services are neither unique identifiers nor guaranteed to be static. They should not be used to identify an Endpoint. In addition to new algorithms for naming, the customer can rename the endpoints at will, and shall not be constrained or limited in providing names meaningful to them. **For Windows MIDI Services, the customer is always in control of their setup.**

## What Identifier should I use to find a device?

For Windows MIDI Services, the primary identifier for an endpoint on a single PC is `MidiEndpointDeviceInformation.EndpointDeviceId`. We do everything possible to keep this string consistent within the limitations of the device implementation (see notes below about USB). Although we attempt to make it portable when we can, it's not guaranteed to be portable to other PCs. If you need further identification information, please read on.

Apps sometimes need to load specialized templates based upon the make/model of the device, and so have different identification requirements. The `EndpointDeviceId` is not portable across PCs. Although MIDI 2.0 includes MIDI Capability Inquiry (MICI-CI) as the correct long-term approach for discovering device capabilities, many devices today do not support MIDI-CI, and so there are other identifiers listed below which may be useful to you. In-particular, VID/PID for USB devices.

## A note about USB and uniquely identifying a device

Many USB devices lack a unique serial number. Some indicate they have a unique serial number, but instead contain a placeholder which is identical from device to device. As a result, when a customer unplugs a USB device and then plugs it into a different USB port, or into a hub, operating systems (including Windows) often see the device as a new device. All operating systems use heuristics to try to identify the device and keep the properties, but without unique serial numbers, this problem is compounded when a customer has two or more of hte same device attached to the computer.

I wrote an article about `iSerialNumber` here, and have evangelized its use with developers creating MIDI 2.0 devices so that this problem will be less of a concern there.

[The importance of including a unique iSerialNumber in your USB MIDI Devices](https://devblogs.microsoft.com/windows-music-dev/the-importance-of-including-a-unique-iserialnumber-in-your-usb-midi-devices/)

That's all to say that we can try our best to associate data with USB MIDI devices lacking a proper `iSerialNumber`, but there is no guarantee that the metadata, including unique identifiers, will be sticky. 

# What other identifiers are available?

## All MIDI 2.0 devices

As mentioned above, the primary identifier is the `MidiEndpointDeviceInformation.EndpointDeviceId` property. At any one point in time, there cannot be more than one endpoint with the same identifier attached to the same PC.

Additionally, the device information includes other identifiers which may be useful to you.

| Property | Source | Notes |
| -------- | ------ | ------------------ |
| `MidiEndpointDeviceInformation.EndpointDeviceId` | Windows | The primary identifer for the endpoint on a single PC |
| `MidiEndpointDeviceInformation.DeviceInstanceId` | Windows | The identifier for the parent device for this endpoint. In USB devices with an `iSerialNumber`, this typically contains the VID, PID, and iSerialNumber for the device. |
| `MidiDeclaredEndpointInfo.ProductInstanceId` | UMP Endpoint discovery | This is the unique id that the endpoint declares. It is not guaranteed to be unique across brands or models, and is an optional but recommended part of MIDI 2.0 discovery. In the case of a single device with multiple endpoints (as can happen with Network MIDI 2.0) it is shared across all endpoints for the device. Nevertheless, this is at least as reliable as the old WinMM port names. |
| `MidiDeclaredDeviceIdentity.*` | UMP Endpoint discovery | This contains the SysEx Ids, Device Family and Device Model Ids as declared by the device. When provided (it is optional) it is expected to remain consistent |

## USB MIDI 2.0

The DDI for the USB MIDI 2.0 implementation on Windows includes the ability to request the VID, PID, Serial Number, and Manufacturer Name for a device. This makes it easy for us to provide that information as part of the device information.

For USB devices, the VID/PID are a good combination for recognizing a type of USB device. If a serial number is provided, combined with the VID/PID, it's a good combination for recognizing a specific USB device.

| Property | Source | Notes |
| -------- | ------ | ------------------ |
| `MidiEndpointTransportSuppliedInfo.VendorId` | USB Driver | The USB VID for the parent device for this endpoint. If VID and PID are both 0, the information was not provided. |
| `MidiEndpointTransportSuppliedInfo.ProductId` | USB Driver | The USB PID for the parent device for this endpoint. If VID and PID are both 0, the information was not provided. |
| `MidiEndpointTransportSuppliedInfo.SerialNumber` | USB Driver or other | The USB `iSerialNumber`, if provided, for the parent of this endpoint. May also contain other provided serial numbers when the device is not a USB device. |

## USB MIDI 1.0 Ports as UMP Endpoints

MIDI 1.0 doesn't have any in-protocol way to discover unique identifiers, so we must rely on other properties. Windows MIDI Services, and Windows in general, does not strongly distinguish between a USB MIDI 1.0 device, and other MIDI 1.0 devices for which a Kernel Streaming (KS) driver has been provided. However, in the case of USB devices, we do have additional information we read from the device.

When creating an aggregate endpoint for MIDI 1.0 ports on a device, we do the following in the service, if the device is using a MIDI 1.0 driver:

- We combine together all declared KS interfaces with the device into one set.
- We enumerate all active input and output KS pins and generate MIDI Groups from them.
- We take all the enumerated group/pin combos and create a set of single-direction `MidiGroupTerminalBlock` structures that mirror those used by USB MIDI 2.0 devices (we use the same structure and property). Each Group Terminal Block in a MIDI 1.0 device can only span a single group, so the `FirstGroup` property is always the group, and `GroupCount` is always 1.

If the device is attached to the new MIDI 2.0 driver, we follow a very similar process, but the driver itself creates the Group Terminal Blocks and we present them in the same way.

> The Group Terminal Block indexes would only change if a device supports dynamically reassigning or enabling/disabling jacks/pins in their configuration app. There is really nothing that can be reasonably done at that point to help a MIDI 1.0 app identify the group to use. MIDI 2.0-aware apps need to support dynamically enabled/disabled groups regardless.

Unlike with MIDI 2.0, the group indexes included in an aggregated MIDI 1.0 endpoint, are expected to stay consistent, with the above caveat. Those are generated when Windows enumerates the active pins for a device, and are sequentially created with Group indexes starting at 0 for input and 0 for output. The group indexes themselves are not the same as the Jack or Pin IDs declared by the device, because that would lead to confusing gaps in numbering.

| Property | Source | Notes |
| -------- | ------ | ------------------ |
| `MidiEndpointDeviceInformation.EndpointDeviceId` | Windows | The primary identifer for the endpoint on a single PC, with USB caveats as explained above |
| `MidiEndpointDeviceInformation.DeviceInstanceId` | Windows | Same use as for USB MIDI 2.0 devices |
| `MidiGroupTerminalBlock.FirstGroup` | Driver or service | Although the actual indexes are not the same, this ultimately maps to a KS Pin / USB Jack through an internal pin map. Each Group is a single MIDI 1.0 port |

The equivalent of a MIDI 1.0 Port in WinMM, in this case, would be the `EndpointDeviceId` + the `FirstGroup.Index`.

## MIDI Loopback and Virtual Device Endpoints (Windows MIDI Services)

The unique identifiers for these are specified at creation time. As long as the user or application is consistent in those values, the devices will have the same `EndpointDeviceId` each time, even across PCs.

For third-party loopback or virtual devices using their own drivers, we do not control how those devices are created or what information they provide, and so can offer no guidance there.

# Future Transports

## Network MIDI 2.0 devices

Network MIDI 2.0 is in development. More information will be provided when the transport has been completed.

| Property | Source | Notes |
| -------- | ------ | ------------------ |
| `UMPEndpointName` | mDNS and/or UMP Endpoint Discovery | `UMPEndpointName` shall be unique within a device's endpoints. The specification indicates that the combination of the `UMPEndpointName` and `ProductInstanceId` may be used to reconnect to a device and recall device properties. However, some network devices allow renaming so keep that in mind. |
| `ProductInstanceId` | mDNS and/or UMP Endpoint Discovery | See note above. |

Note that IP addresses and ports are not persistent identifiers for remote hosts/clients.

## Other MIDI 1.0 devices

Information on upcoming BLE MIDI 1.0 will be provided when the transports are available.

# Scenarios

## I need to reconnect to a MIDI device between sessions on the same PC

If your app needs to reconnect to a MIDI device on the same PC, use the `EndpointDeviceId` as the preferred string to store. The GUID at the end of it will be the same each time (it represents the UMP endpoint type), so it may be left off when stored if you need to save space. Please note the caveats at the top about how we try to keep this number consistent, but have some constraints due to different device implementations.

Do not use the Endpoint Name, Function Block Names, Group Terminal Block Names, as the user can easily rename the device in different ways.

## I need to reconnect to a USB MIDI device across PCs (Studio/Gig)

If a USB devices has an `iSerialNumber` the generated `EndpointDeviceId` should be the same. You can also use VID/PID/Serial in the case of USB MIDI 1.0 and USB MIDI 2.0 devices, and other information described above (including Product Instance Id) in the case of any MIDI 2.0 devices.

## I need to reconnect to a USB MIDI device across PC and Mac

This will be a little more difficult given that the main identifier is not the same across Windows and Mac. Similarly, the WinMM port names have never been identical across Windows and Mac. Therefore, you can use the same heuristics you use today, or else consider VID/PID/Serial when available for USB devices.

## I need to load specific templates or features for a make/model of USB MIDI device

For devices which support it, prefer MIDI-CI as the mechanism for identifying a device and its capabilities.

Additionally, the usual [MIDI 1.0 SysEx Identity Request](http://midi.teragonaudio.com/tech/midispec/identity.htm) can provide this information to you for many devices. The information provided in this is approximately the same as what is provided in MIDI 2.0 discovery, for the Device Identity.

For USB MIDI (1.0 and 2.0) devices, the VID and PID will identify the make and model of the device for you when they are available.

> NOTE: The VID/PID aren't available for MIDI 1.0 devices at the time of this writing, but it's being worked on before the official production release.
