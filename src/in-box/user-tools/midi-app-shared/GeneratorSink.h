// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <cstdint>
#include <functional>

namespace midiapp
{
    // Where a generator's messages go when they are not sent straight to a connection: one
    // message at a time, with the timestamp it is meant to play at. Called on the generator's own
    // thread, ahead of that timestamp, so it must not block for long.
    using GeneratorSink = std::function<void(uint64_t timestamp, uint32_t const* words, uint32_t wordCount)>;
}
