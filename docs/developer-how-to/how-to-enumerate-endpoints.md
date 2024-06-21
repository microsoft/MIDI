---
layout: page
title: Enumerate UMP Endpoints
parent: Developer How-to
has_children: false
---

# How to Enumerate UMP Endpoints

Sometimes, an application wants to get a snapshot of active endpoints. For example, the MIDI console application does this when it presents a list of devices to pick from. This process is called enumeration.

> Enumerating endpoints one time is good only for a brief snapshot listing. If you want to have a list of devices that is always up to date when devices are plugged/unplugged, come into range, have property updates, etc., see the [how to watch endpoints](./how-to-watch-endpoints.md) topic.

```cpp
auto endpointList = MidiEndpointDeviceInformation::FindAll();
```

That is the equivalent of passing in a sort order of the name, and a filter of the two types of UMP endpoints that an application will work with. (All endpoints are presented through the API/SDK as UMP, but their native data format is part of the device information and part of the filter options.)

```cpp
auto endpointList = MidiEndpointDeviceInformation::FindAll(
    MidiEndpointDeviceInformationSortOrder::Name,
    MidiEndpointDeviceInformationFilters::AllStandardEndpoints
);
```

The application may then iterate through the list, reading the properties as needed.

## UMP Endpoint properties

Windows MIDI Services has a very rich set of properties available for a UMP Endpoint. This information includes hardware and other transport information, parent device information, user-supplied information, and in the case of a MIDI 2.0 UMP Endpoint, declared information from endpoint discovery and protocol negotiation carried out within the Windows service.

For more details, see the [`MidiEndpointDeviceInformation`](../sdk-winrt-core/enumeration/MidiEndpointDeviceInformation.md) class documentation. You may also use the [MIDI Console application](../console/midi-console.md) to see all of the properties (including the raw property data if you choose to) for an endpoint.

## Sample Code

* [C++ Sample to Statically Enumerate Endpoints](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/static-enum-endpoints)
