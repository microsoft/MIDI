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
// only ever a request to navigate. Nothing here mutes, unmutes, creates or removes a loopback:
// those stay behind a deliberate click inside the app. Treat everything in the URI as untrusted.

#define MIDI_LOOPBACK_SETUP_PROTOCOL_SCHEME         L"ms-midi-loopback-setup"

// Show the loopbacks which were muted because MIDI was feeding back into them.
#define MIDI_LOOPBACK_SETUP_PROTOCOL_URI_FEEDBACK   L"ms-midi-loopback-setup:feedback"

#define MIDI_LOOPBACK_SETUP_PROTOCOL_PATH_FEEDBACK  L"feedback"
