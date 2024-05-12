---
layout: api_group_page
title: Metadata
parent: Microsoft.Devices.Midi2
has_children: true
---

# Metadata

The Windows Service intercepts (but does not remove from the stream) Endpoint metadata notifications. For example, we'll intercept Endpoint Name notifications and use those to provide a new endpoint-supplied name for the device. These are cached in the SWD properties for the Endpoint Device.

In addition to the endpoint data, we also capture and store block data. The block data should be used by applications to identify which groups are active and how to display them to the user. For example, you may want to display a function block name including group numbers like "Sequencer (Groups 1, 2, 3)" in a way similar to how you treated ports in the past.

Function Blocks and Group Terminal Blocks are important types of MIDI 2.0 metadata which describe an endpoint and so have their own discrete types.
