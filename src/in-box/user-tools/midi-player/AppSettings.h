// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midiplayer
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        // The endpoint last played to. Empty on a first run, which is when the in-box synth is
        // chosen instead.
        std::wstring const& LastEndpointDeviceId() const noexcept { return m_lastEndpointDeviceId; }
        void LastEndpointDeviceId(_In_ std::wstring const& value) noexcept;

        uint8_t GroupIndex() const noexcept { return m_groupIndex; }
        void GroupIndex(uint8_t value) noexcept;

        bool RepeatQueue() const noexcept { return m_repeatQueue; }
        void RepeatQueue(bool value) noexcept;

        bool ShowQueue() const noexcept { return m_showQueue; }
        void ShowQueue(bool value) noexcept;

    private:
        AppSettings() noexcept;

        std::wstring m_lastEndpointDeviceId{};
        uint8_t m_groupIndex{ 0 };
        bool m_repeatQueue{ false };
        bool m_showQueue{ true };
    };
}
