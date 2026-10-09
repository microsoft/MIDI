// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Follows messages through a patch without a device, for the editor's trace. It compiles the
// patch and runs each message the way routing does, so what it shows is what routing would do.
// Pure, so the tests can prove that.

#include "PatchDocument.h"
#include "LogicSteps.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace midipatchbay
{
    // As many messages as a trace takes. More is a list nobody reads.
    constexpr size_t MaximumTraceMessages = 32;

    struct TraceMessage
    {
        std::array<uint32_t, MaximumUmpWords> Words{};
        uint8_t WordCount{ 0 };
    };

    // What a trace can make without typing words.
    enum class TraceMessageKind : int32_t
    {
        NoteOn = 0,
        NoteOff = 1,
        ControlChange = 2,
        ProgramChange = 3,
        PitchBend = 4,
        ChannelPressure = 5,
    };

    // Group and channel from 0. Number is the note, controller or program. Value is a velocity, a
    // controller value or a pressure from 0 to 127, or a pitch bend from 0 to 16383. A MIDI 2.0
    // message gets the value at full resolution, scaled the way MIDI 2.0 scales.
    TraceMessage MakeTraceMessage(
        _In_ TraceMessageKind kind,
        _In_ uint8_t group,
        _In_ uint8_t channel,
        _In_ uint32_t number,
        _In_ uint32_t value,
        _In_ bool midi2) noexcept;

    // One to four words of hex, with or without 0x, between spaces or commas. Nothing else.
    std::optional<TraceMessage> ReadTraceWords(_In_ std::wstring_view text) noexcept;

    // The same message on another group. A message without a group is left as it is.
    TraceMessage WithTraceGroup(_In_ TraceMessage const& message, _In_ uint8_t group) noexcept;

    // A message reaching a destination endpoint, as it would be sent there.
    struct TraceArrival
    {
        std::wstring EndpointId{};
        TraceMessage Message{};
    };

    // Where one message went.
    struct TraceStep
    {
        // The endpoints and steps it reached, by node id, in the order it reached them.
        std::vector<std::wstring> Nodes{};

        // The links it went along, by id.
        std::vector<std::wstring> Links{};

        // The steps that kept it out, on one path or more.
        std::vector<std::wstring> KeptOut{};

        std::vector<TraceArrival> Arrivals{};

        // Every memory in the patch after this message, by the name the patch gives it.
        std::vector<std::pair<std::wstring, LogicValue>> Memories{};
    };

    enum class TraceOutcome : int32_t
    {
        Traced = 0,

        // A loop between steps, or a patch too large: it doesn't route, so there is nothing to follow.
        DoesNotRoute = 1,

        // Nothing leaves the endpoint the messages come in from.
        NothingFromSource = 2,
    };

    struct TraceResult
    {
        TraceOutcome Outcome{ TraceOutcome::Traced };

        // One for each message, in order.
        std::vector<TraceStep> Steps{};
    };

    // Sends the messages in order into the patch from one endpoint, the way routing would. Every
    // endpoint on the canvas counts as present and muted links are left out. A step remembers
    // from one message to the next, so a note off follows its note on, and memories start empty
    // and carry on the same way. What comes after a throttle runs at once. Generators don't run.
    TraceResult TracePatch(
        _In_ PatchDocument const& patch,
        _In_ std::wstring const& sourceNodeId,
        _In_ std::vector<TraceMessage> const& messages) noexcept;
}
