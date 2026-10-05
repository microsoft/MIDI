// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midisoundfontsynth
{
    // Appearance and placement come from the shared settings. The rest is about where the sound
    // goes and how the app behaves when its window is closed.
    class AppSettings final : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        // Empty follows the Windows default output.
        std::wstring const& AudioDeviceId() const noexcept { return m_audioDeviceId; }
        void AudioDeviceId(_In_ std::wstring const& value) noexcept;

        bool ExclusiveMode() const noexcept { return m_exclusiveMode; }
        void ExclusiveMode(_In_ bool value) noexcept;

        uint32_t ExclusiveBufferMilliseconds() const noexcept { return m_exclusiveBufferMilliseconds; }
        void ExclusiveBufferMilliseconds(_In_ uint32_t value) noexcept;

        bool MinimizeToNotificationArea() const noexcept { return m_minimizeToNotificationArea; }
        void MinimizeToNotificationArea(_In_ bool value) noexcept;

        bool StartMinimized() const noexcept { return m_startMinimized; }
        void StartMinimized(_In_ bool value) noexcept;

        // The folder the add dialog opened in last, so a library of banks is one click away.
        std::wstring const& LastSoundFontFolder() const noexcept { return m_lastSoundFontFolder; }
        void LastSoundFontFolder(_In_ std::wstring const& value) noexcept;

    private:
        AppSettings() noexcept;

        std::wstring m_audioDeviceId{};
        bool m_exclusiveMode{ false };
        uint32_t m_exclusiveBufferMilliseconds{ 10 };
        bool m_minimizeToNotificationArea{ false };
        bool m_startMinimized{ false };
        std::wstring m_lastSoundFontFolder{};
    };
}
