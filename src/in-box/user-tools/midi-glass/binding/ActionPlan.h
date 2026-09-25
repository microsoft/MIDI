// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h, XAML and the MIDI SDK. Everything here is the arithmetic of
// turning a sequence into a list of things to do at known times, which is testable without a
// clock and without a device. Running it is the runtime layer's job.

#include <sal.h>
#include <cstdint>
#include <string>
#include <vector>

#include "LayoutModel.h"
#include "BindingEngine.h"

namespace glass
{
    // What one step of a plan does.
    enum class ActionKind
    {
        // Words already built, handed straight to a connection. One channel voice message, or a
        // whole system exclusive dump packetized into UMP.
        Send = 0,

        // Do nothing for this long. The only reason a plan has a clock at all: a firmware dump
        // needs real gaps between packets, and a drum fill needs real gaps between notes.
        Wait = 1,

        // Move another control on the surface. What makes a button able to reset a page of
        // faders without the customer wiring eight messages by hand.
        SetControlValue = 2,

        // Show another page.
        GoToPage = 3,
    };

    struct PlanAction
    {
        ActionKind Kind{ ActionKind::Send };

        int32_t DestinationIndex{ -1 };

        // A system exclusive dump is thousands of packets, so this is a vector rather than the
        // fixed four words a channel voice message needs.
        std::vector<uint32_t> Words{};

        uint32_t WaitMilliseconds{ 0 };

        // -1 when the step names something this layout does not have. Kept rather than dropped
        // so the editor can still show the step and say what is wrong.
        int32_t TargetControlIndex{ -1 };
        double TargetValue{ 0.0 };

        int32_t TargetPageIndex{ -1 };
    };

    // One thing a control does when it is touched, released, turned on or turned off, beyond the
    // channel voice messages the binding engine sends immediately.
    struct ActionPlan
    {
        std::vector<PlanAction> Actions{};

        // Runs again from the top when it reaches the end. A patch recall does not; an arpeggio
        // held under a finger does.
        bool Loops{ false };

        // Lifting the finger stops it. The other looping mode stops on the next press instead,
        // and the two behave differently enough that the runner has to be told which.
        bool StopsOnRelease{ false };
    };

    // A plan cannot grow without limit. A repeat block of a thousand around a repeat block of a
    // thousand is a layout from a stranger, not a mistake, and it must not be able to make this
    // allocate forever.
    constexpr size_t MaximumPlanActions = 4096;

    // The most words one action can carry. 512 KB of system exclusive is about 87,400 UMP words,
    // which is fine to hold and is sent in whatever bites the connection accepts.
    constexpr size_t MaximumPlanWords = 128 * 1024;

    // System exclusive bytes to UMP data messages, six bytes to a packet.
    //
    // The leading 0xF0 and trailing 0xF7 are stripped if they are there. A MIDI file, a manual
    // and a forum post all disagree about whether to include them, and somebody pasting a dump
    // should not have to know which convention this app chose.
    //
    // Returns false and writes nothing when a byte outside the payload has its top bit set,
    // because a truncated system exclusive message can leave a synthesizer waiting for an end
    // that never comes, and half a firmware image is worse than none.
    bool PacketizeSystemExclusive(
        _In_ std::vector<uint8_t> const& bytes,
        _In_ uint8_t groupIndex,
        _Out_ std::vector<uint32_t>& words) noexcept;

    // Everything a layout's controls do that is not an immediate channel voice message, resolved
    // to indexes once at load. Looked up by control and trigger on the hot path.
    class ActionPlanSet
    {
    public:
        void Prepare(
            _In_ LayoutDocument const& document,
            _In_ std::vector<PreparedDestination> const& destinations) noexcept;

        // Null when this control does nothing for this trigger, which is the common case.
        ActionPlan const* Find(
            _In_ size_t controlIndex,
            _In_ MessageTrigger trigger) const noexcept;

        // The shortest a looping plan may take to come round again, so a plan with no waits in
        // it cannot spin the clock thread.
        static constexpr uint32_t MinimumLoopMilliseconds = 10;

        size_t PlanCount() const noexcept { return m_plans.size(); }

        void Clear() noexcept;

    private:
        static constexpr size_t TriggerCount = 5;

        // control index * TriggerCount + trigger, holding an index into m_plans or -1.
        std::vector<int32_t> m_index{};
        std::vector<ActionPlan> m_plans{};
    };
}
