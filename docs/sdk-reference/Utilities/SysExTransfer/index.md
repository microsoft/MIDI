---
layout: sdk_namespace_page
title: WinRT API System Exclusive Transfer Utilities
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
description: Namespace with types for sending System Exclusive data to an endpoint
---

Sending System Exclusive (SysEx) data, such as a patch dump or a firmware update, is a common MIDI job. The data is often much bigger than one message, and devices often need time to handle what they receive. So the transfer has to be split into separate messages, and sent slowly enough that the receiving device can keep up.

The types in this namespace do that work for you. They read the source data, turn it into UMP messages, send it through an open `MidiEndpointConnection`, and report progress along the way. `MidiSystemExclusiveReceiver` does the reverse, and collects incoming SysEx into whole messages.
