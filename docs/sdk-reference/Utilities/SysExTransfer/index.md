---
layout: sdk_namespace_page
title: App SDK System Exclusive Transfer Utilities
namespace: Windows.Devices.Midi2.Utilities.SysExTransfer
description: Namespace with types for sending System Exclusive data to an endpoint
---

Transferring System Exclusive (SysEx) data, such as a patch dump or a firmware update, is a common MIDI operation. Because the data is often much larger than a single message, and because devices frequently need time to process what they receive, the transfer has to be broken up into individual messages and paced so the receiving device is not overwhelmed.

The types in this namespace handle that work for you: reading the source data, converting it into UMP messages, sending it through an open `MidiEndpointConnection`, and reporting progress as the transfer proceeds.
