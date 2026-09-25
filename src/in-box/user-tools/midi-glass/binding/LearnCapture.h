// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and the MIDI SDK. Reading a message back into the five things
// a binding needs is arithmetic, and it is tested as arithmetic.

#include <sal.h>
#include <cstdint>
#include <string>

#include "LayoutModel.h"

namespace glass
{
    // What touching a control on hardware tells us. Five things, not one: which endpoint it
    // arrived on, which group, which channel, which kind of message and which number.
    //
    // Capturing only the number is why remapping a controller in most software is an hour of
    // typing. Capturing all five is why it should be a minute.
    struct LearnedBinding
    {
        std::wstring DeviceName{};

        MessageKind Kind{ MessageKind::ControlChange };

        int32_t GroupIndex{ 0 };
        int32_t ChannelIndex{ 0 };
        uint32_t Number{ 0 };

        // Where the control was when it arrived, 0 to 1. Not part of the binding; it is what
        // lets the editor show the value moving while somebody wiggles the right knob.
        double Value{ 0.0 };
    };

    // Which of the five fields a capture is allowed to write. Somebody remapping within one
    // device locks the endpoint and takes only the number.
    struct LearnAcceptance
    {
        bool Device{ true };
        bool Group{ true };
        bool Channel{ true };
        bool Kind{ true };
        bool Number{ true };
    };

    // One message read back into a binding. False for anything that is not a channel voice
    // message this app can bind to, which is most of what arrives on a busy port: clock, active
    // sensing, stream messages and system exclusive all go past without arming anything.
    bool TryLearnFromWords(
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint32_t wordCount,
        _Out_ LearnedBinding& learned) noexcept;

    // Whether this message is worth acting on while learning. A note off, a control change at
    // zero and a pitch bend passing through center all arrive constantly while somebody is
    // reaching for the knob they actually mean, and taking the first of them would bind the
    // wrong thing.
    bool IsWorthLearning(_In_ LearnedBinding const& learned) noexcept;

    // Writes the accepted fields onto a message, leaving the rest as they were.
    void ApplyLearned(
        _In_ LearnedBinding const& learned,
        _In_ LearnAcceptance const& accept,
        _Inout_ ControlMessage& message) noexcept;

    // The same, for what a control listens to.
    void ApplyLearned(
        _In_ LearnedBinding const& learned,
        _In_ LearnAcceptance const& accept,
        _Inout_ FeedbackBinding& feedback) noexcept;

    // Whether two captures are the same binding, so holding a knob still does not fill eight
    // controls with eight copies of it during a bank learn.
    bool IsSameBinding(_In_ LearnedBinding const& left, _In_ LearnedBinding const& right) noexcept;
}
