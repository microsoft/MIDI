// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiTimeCode.h"

namespace midiclock
{
    // What a saved clock sends. MIDI Time Code has no tempo at all, so it is a kind of clock
    // rather than a switch on a beat clock: the tempo, the divider and the swing simply do not
    // apply to it, and the editor hides them.
    enum class ClockKind : int32_t
    {
        BeatClock = 0,
        TimeCode = 1
    };

    // Group value meaning "every group the endpoint declares", resolved against the endpoint
    // when the clock starts rather than stored as a list.
    constexpr int32_t AllDeclaredGroups = -1;

    constexpr double MinimumBeatsPerMinute = 20.0;
    constexpr double MaximumBeatsPerMinute = 300.0;
    constexpr double DefaultBeatsPerMinute = 120.0;

    // Standard MIDI beat clock. Kept in the file so it can be changed by hand for testing,
    // the way midi.exe send-beat-clock allows, without putting it in front of the customer.
    constexpr int32_t DefaultPulsesPerQuarterNote = 24;
    constexpr int32_t MinimumPulsesPerQuarterNote = 1;
    constexpr int32_t MaximumPulsesPerQuarterNote = 96;

    // The output rate against the tempo, as a ratio. 1/1 is the plain clock; 1/2 halves the
    // rate so the receiver runs at half speed, 2/1 doubles it, 3/2 is triplets and 2/3 dotted.
    constexpr int32_t DefaultClockRatioNumerator = 1;
    constexpr int32_t DefaultClockRatioDenominator = 1;
    constexpr int32_t MaximumClockRatioPart = 64;

    // 50 is straight. The tempo is unchanged either way: swing lengthens the first of each
    // pair of notes and shortens the second by the same amount.
    constexpr double DefaultSwingPercent = 50.0;
    constexpr double MinimumSwingPercent = 50.0;
    constexpr double MaximumSwingPercent = 75.0;

    // What gets swung, as a division of the quarter note: 2 is eighth notes, 4 is sixteenths.
    constexpr int32_t DefaultSwingSubdivision = 2;

    constexpr double MaximumOffsetMilliseconds = 500.0;

    // A saved clock. Everything here comes from a machine-wide, user-writable file, so every
    // field is range checked on the way in.
    struct ClockDefinition
    {
        std::wstring Id{};
        std::wstring Name{};
        double BeatsPerMinute{ DefaultBeatsPerMinute };
        std::wstring EndpointDeviceId{};

        // Last known name of the endpoint, so a tile still says where it points when the
        // device is unplugged.
        std::wstring EndpointName{};

        int32_t GroupIndex{ 0 };
        int32_t PulsesPerQuarterNote{ DefaultPulsesPerQuarterNote };
        bool SendStartStop{ true };
        int32_t DisplayOrder{ 0 };

        int32_t ClockRatioNumerator{ DefaultClockRatioNumerator };
        int32_t ClockRatioDenominator{ DefaultClockRatioDenominator };
        double SwingPercent{ DefaultSwingPercent };
        int32_t SwingSubdivision{ DefaultSwingSubdivision };
        double OffsetMilliseconds{ 0.0 };

        ClockKind Kind{ ClockKind::BeatClock };

        // Time code only. Ignored by a beat clock.
        midiapp::MidiTimeCodeFrameRate FrameRate{ midiapp::MidiTimeCodeFrameRate::Frames30 };
        midiapp::MidiTimeCodePosition StartTimeCode{};
        bool SendFullFrameMessages{ true };
    };

    // The saved clocks, in their own file in a subfolder of the Windows MIDI Services
    // configuration folder. The service never reads it; this is the app's own state.
    //
    // Nothing here throws. A failure leaves the file untouched and is reported through
    // LastErrorMessage so the window can tell the customer rather than fail silently.
    class ClockStore
    {
    public:
        static ClockStore& Current() noexcept;

        std::wstring const& Path() const noexcept { return m_path; }
        winrt::hstring LastErrorMessage() const noexcept { return m_lastError; }

        // A file that is not there yet is not a failure: the app simply starts with no clocks.
        bool Load() noexcept;
        bool Save() noexcept;

        std::vector<ClockDefinition> const& Clocks() const noexcept { return m_clocks; }

        ClockDefinition const* Find(_In_ std::wstring const& id) const noexcept;

        // Matches on the endpoint a clock points at, for the command line hand-off.
        ClockDefinition const* FindByEndpoint(_In_ std::wstring const& endpointDeviceId) const noexcept;

        // Adds when the id is new, replaces when it already exists. Returns the stored id.
        std::wstring Upsert(_In_ ClockDefinition definition) noexcept;

        void Remove(_In_ std::wstring const& id) noexcept;

        bool IsFull() const noexcept { return m_clocks.size() >= MaximumClockCount; }

        static std::wstring NewId() noexcept;

        // A hand-edited file could otherwise ask the app to build an unbounded number of tiles.
        static constexpr size_t MaximumClockCount = 128;

    private:
        ClockStore() noexcept;

        void ResolveDefaultPath() noexcept;

        std::wstring m_path{};
        std::wstring m_folder{};
        std::vector<ClockDefinition> m_clocks{};
        winrt::hstring m_lastError{};
    };
}
