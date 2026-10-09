// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Answers MIDI-CI Property Exchange requests for one synthesizer, the way the in-box General MIDI
// synthesizer transport does. Kept out of the engine library because it parses request headers
// with Windows.Data.Json, and the engine has no WinRT in it.

#pragma once

#include "SynthCore.h"

namespace SoundFontSynth
{
    // Worker only. Each call does at most one thing: answers one request, or sends one chunk of a
    // reply already in progress, or starts telling subscribers that the channel list changed. A
    // long reply is paced across calls so it never floods the endpoint.
    //
    // Parsing JSON through WinRT needs a COM apartment on the calling thread.
    void ServicePropertyRequests(_In_ SynthCore& core, _In_ ISysExSink& wire) noexcept;
}
