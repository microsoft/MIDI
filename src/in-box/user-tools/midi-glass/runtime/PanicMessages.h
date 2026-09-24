// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, so the exact set of messages a panic sends is pinned down
// by a test rather than by whoever reads the code next.

#include <sal.h>
#include <cstdint>
#include <span>

namespace glass
{
    // Sustain off, all notes off, all sound off, pitch bend center. In that order, because
    // releasing the pedal before asking for silence is what makes the silence stick.
    constexpr uint32_t PanicWordsPerChannel = 4;

    constexpr uint8_t PanicChannelCount = 16;

    // Built as MIDI 1.0 protocol on purpose. A panic has to work on the oldest thing plugged in,
    // and every one of these four is a MIDI 1.0 channel voice message that any device understands.
    // The service still converts it for whatever is on the other end.
    uint32_t BuildPanicWords(
        _In_ uint8_t group,
        _In_ uint8_t channel,
        _Inout_ std::span<uint32_t> words) noexcept;

    // Every channel of one group, so a caller only has to walk groups.
    uint32_t BuildPanicWordsForGroup(
        _In_ uint8_t group,
        _Inout_ std::span<uint32_t> words) noexcept;
}
