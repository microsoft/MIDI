---
layout: api_group_page
title: Endpoint Enumeration
parent: Microsoft.Devices.Midi2
has_children: true
---

# Enumerating Endpoints

Windows MIDI Services provides detailed information about each MIDI endpoint on the system. In addition to ids and names, you can also get user metadata, function blocks, group terminal blocks, in-protocol properties, the parent device and container, and much more.

There are two ways to enumerate endpoint devices.

1. Static enumeration using the `MidiEndpointDeviceInformation` class. This is a snapshot in time and is not updated when in-protocol information is updated, or the user has specified new properties like the name. This approach is really only useful in the simplest of scenarios, as it does not handle device connects and disconnects after the initial enumeration.
2. Dynamic enumeration using the `MidiEndpointDeviceWatcher`. When you set up a watcher on a background thread, you will be notified when any new endpoints are connected to the system, or any existing endpoints are disconnected. You will also be alerted when properties change on an enumerated device. For example, when new function block information is sent in-protocol, the properties are updated and an event is raised. For these reasons, the device watcher approach is the approach any non-trival application should use to list and track MIDI endpoints.

> Note that you can enumerate endpoint devices using the stock `Windows.Devices.Enumeration.DeviceInformation` and `Windows.Devices.Enumeration.DeviceWatcher` classes. However, those classes do not automatically request the extended property set needed for MIDI, do not translate the binary properties like the group terminal blocks and function blocks, and also do not automatically resolve the relationship with the parent device.
