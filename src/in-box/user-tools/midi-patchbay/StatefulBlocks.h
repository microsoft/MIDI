// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The steps that remember something between messages, or that send a message somewhere other
// than every connection out of them. Pure, so the unit tests compile this exactly as it ships.

#include "CapabilityInquiry.h"
#include "LogicSteps.h"

#include <array>
#include <atomic>
#include <memory>

namespace midipatchbay
{
    struct Voice
    {
        bool Sounding{ false };
        bool Midi2{ false };
        uint8_t Group{ 0 };
        uint8_t Channel{ 0 };
        uint8_t Note{ 0 };
        uint32_t Age{ 0 };
    };

    // What a step keeps between messages. One for each step, shared by every path through it and
    // kept when the patch changes, so it is safe from any routing thread.
    struct BlockState
    {
        // Clock divider.
        std::atomic<uint32_t> Count{ 0 };

        // Gate: bit 0 is set while it is the other way from how it starts.
        std::atomic<uint32_t> Flags{ 0 };

        // (N)RPN filter and transform: for each group and channel, the parameter that MIDI 1.0
        // control changes have selected.
        std::array<std::atomic<uint32_t>, 256> Parameters{};

        // Note distributor. The lock is held for the few steps of one message, never while
        // anything is sent.
        std::atomic_flag VoiceLock{};
        std::array<Voice, MaximumVoices> Voices{};
        uint32_t VoiceCount{ 0 };
        uint32_t NextVoice{ 0 };
        uint32_t VoiceClock{ 0 };
        int32_t LatestVoice{ -1 };

        // MIDI-CI filter.
        CiFilterMemory CiFilter{};

        // MIDI-CI responder. Made with the step, never on the way of a message.
        std::unique_ptr<CiResponderState> Ci{};

        // Branch and Switch: where notes, held pedals and long messages went. Made with the step.
        std::unique_ptr<WayMemory> Ways{};
    };

    // Makes what a step of this kind needs before any message reaches it.
    void PrepareBlockState(_In_ BlockKind kind, _Inout_ BlockState& state);

    constexpr uint8_t MaximumStageWords = 4;
    constexpr size_t MaximumStageMessages = 4;

    // Every connection out of the step, rather than one of them.
    constexpr int32_t EveryEdge = -1;

    struct StageMessage
    {
        std::array<uint32_t, MaximumStageWords> Words{};
        uint8_t Count{ 0 };
        int32_t Edge{ EveryEdge };
    };

    // What a step sends instead of the message it was given.
    struct StageOutput
    {
        std::array<StageMessage, MaximumStageMessages> Messages{};
        size_t Count{ 0 };

        void Add(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount, _In_ int32_t edge) noexcept;
    };

    // Clock divider, (N)RPN filter and transform, note distributor, gate, the MIDI-CI steps, and
    // Branch and Switch.
    bool IsStatefulBlock(_In_ BlockKind kind) noexcept;

    // False keeps the message out. True with nothing in the output sends the message, as changed,
    // on every connection out of the step; otherwise what is in the output goes instead. A voice
    // is one connection out, counted in the order they were connected.
    bool RunStatefulBlock(
        _In_ BlockKind kind,
        _In_ BlockSettings const& settings,
        _Inout_ BlockState& state,
        _Inout_updates_(wordCount) uint32_t* words,
        _In_ uint8_t wordCount,
        _In_ uint32_t edgeCount,
        _Inout_ StageOutput& output) noexcept;
}
