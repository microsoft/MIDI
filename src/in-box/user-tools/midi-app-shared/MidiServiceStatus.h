// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    // Whether the Windows MIDI Services service is running right now.
    //
    // Read only, deliberately, and that is the whole point of it existing. The SDK's
    // MidiApi::EnsureServiceAvailable STARTS the service when it is stopped, which is right for
    // an app about to send MIDI and wrong for anything that only wants to report the state:
    // asking the question would change the answer, and a status line would never be able to say
    // the service is stopped.
    //
    // Cheap enough to call on a timer. Never throws.
    bool IsMidiServiceRunning() noexcept;
}
