---
layout: api_group_page
title: Client Plugins
parent: Midi2 core
has_children: true
---

# Client-side Processing Plugins

Connections allocate service resources (time and memory), so we recommend applications maintain only a single connection to an endpoint within any session. But because the new endpoint stream-focused approach aggregates what used to be considered ports, we provide processing plugins to parcel out the incoming messages based on criteria set by the application. In this way, an application can have the logical equivalent of several input ports, without the associated resource usage.

MIDI 1.0 had the concept of ports. Each port was just a single cable/jack from a MIDI stream exposed by the device. The API and driver were responsible for merging all of the different cables into the single stream for outgoing data, or pulling them apart for incoming data.

In MIDI 2.0, what used to be a Port is now morally equivalent to a Group address in the message data. Instead of speaking to N different enumerated entities for a device, the application speaks to a single bidirectional UMP endpoint which aggregates all of this information, much like the driver did behind the scenes in MIDI 1.0. We recognize that there are cases when the old model of MIDI Ports is more convenient for passing around in a DAW or similar app, particularly for incoming data. 

To help, there are plugins which implement `IMidiEndpointMessageProcessingPlugin`. The API includes a few stock plugins, but developers are free to provide their own.

Listener instances are 1:1 with endpoint connections. We don't support using the same listener on multiple endpoints.

