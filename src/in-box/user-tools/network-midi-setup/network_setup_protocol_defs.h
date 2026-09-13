// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// How something outside this app asks it to show the customer a page. Used by the notifications
// app for the button on a toast, and registered by the installer.
//
// A protocol handler is reachable by anything on the machine, including a web page, so a URI is
// only ever a request to navigate. Nothing here approves, denies, connects or removes anything:
// those stay behind a deliberate click inside the app. Treat everything in the URI as untrusted.
//
// One scheme per app, so that a URI naming this app cannot be answered by another one.

#define MIDI_NETWORK_SETUP_PROTOCOL_SCHEME      L"ms-midi-network-setup"

// Show the remote clients which are waiting for a decision.
#define MIDI_NETWORK_SETUP_PROTOCOL_URI_PENDING L"ms-midi-network-setup:pending"

#define MIDI_NETWORK_SETUP_PROTOCOL_PATH_PENDING L"pending"
