---
layout: api_page
title: Connections
parent: Windows.Devices.Midi2 API
has_children: true
---

# Connecting to a MIDI Endpoint

In Windows MIDI Services, once you have opened a session, you will typically open one or more connections to device endpoints. The session class contains methods which return an initialized, but not open, connection to the specified endpoint.

The remainder of your interaction for sending and receiving data is with the `MidiEndpointConnection` class.

All endpoints in Windows MIDI Services send and receive messages using the Universal MIDI Packet format. Any required translation (for MIDI 1.0 devices, for example) is handled in the service and/or in the USB driver.

Workflow

1. Open a session
2. Using an endpoint id discovered through enumeration or another mechanism, create an endpoint connection
3. Wire up to the connection any event handlers or message processors 
4. Open the connection
5. Send and receive messages
6. Using the session, disconnect the connection when you are done with it.

