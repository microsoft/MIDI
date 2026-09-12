// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiclock
{
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
