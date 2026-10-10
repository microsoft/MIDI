// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midisequencer
{
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    // How values are shown in the editors. The file always holds MIDI 2.0 resolution.
    enum class ValueDisplay : uint32_t
    {
        Midi2 = 0,      // 16-bit velocity, 32-bit controllers
        Midi1 = 1,      // 0 to 127
        Percent = 2,
    };

    // Settings that belong to this PC rather than to a sequence, because they name a device or
    // describe how this person likes to work.
    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        // The metronome. The endpoint is kept by id and by name, so it can be found again after
        // the device is plugged into another port.
        bool MetronomeEnabled() const noexcept { return m_metronomeEnabled; }
        void MetronomeEnabled(bool value) noexcept;

        bool MetronomeOnlyWhileRecording() const noexcept { return m_metronomeOnlyWhileRecording; }
        void MetronomeOnlyWhileRecording(bool value) noexcept;

        std::wstring const& MetronomeEndpointId() const noexcept { return m_metronomeEndpointId; }
        std::wstring const& MetronomeEndpointName() const noexcept { return m_metronomeEndpointName; }
        void MetronomeEndpoint(_In_ std::wstring const& id, _In_ std::wstring const& name) noexcept;

        uint8_t MetronomeGroup() const noexcept { return m_metronomeGroup; }
        void MetronomeGroup(uint8_t value) noexcept;

        uint8_t MetronomeChannel() const noexcept { return m_metronomeChannel; }
        void MetronomeChannel(uint8_t value) noexcept;

        uint8_t MetronomeNote() const noexcept { return m_metronomeNote; }
        void MetronomeNote(uint8_t value) noexcept;

        bool ShowLauncher() const noexcept { return m_showLauncher; }
        void ShowLauncher(bool value) noexcept;

        // Pixels for one bar on the timeline, at 100% scaling.
        double BarWidth() const noexcept { return m_barWidth; }
        void BarWidth(double value) noexcept;

        // Snap and launch quantization, in ticks. 0 is off for snap and immediately for launch.
        uint32_t SnapTicks() const noexcept { return m_snapTicks; }
        void SnapTicks(uint32_t value) noexcept;

        uint32_t LaunchQuantizeTicks() const noexcept { return m_launchQuantizeTicks; }
        void LaunchQuantizeTicks(uint32_t value) noexcept;

        // How many bars a take recorded into a launcher slot lasts: 1, 2, 4, 8 or 16.
        uint32_t SlotRecordBars() const noexcept { return m_slotRecordBars; }
        void SlotRecordBars(uint32_t value) noexcept;

        double EditorHeight() const noexcept { return m_editorHeight; }
        void EditorHeight(double value) noexcept;

        ValueDisplay ValuesAs() const noexcept { return m_valuesAs; }
        void ValuesAs(ValueDisplay value) noexcept;

        std::wstring const& LastFolder() const noexcept { return m_lastFolder; }
        void LastFolder(_In_ std::wstring const& value) noexcept;

        static constexpr double DefaultBarWidth = 34.0;
        static constexpr double MinimumBarWidth = 6.0;
        static constexpr double MaximumBarWidth = 2400.0;

    private:
        AppSettings() noexcept;

        bool m_metronomeEnabled{ false };
        bool m_metronomeOnlyWhileRecording{ false };
        std::wstring m_metronomeEndpointId{};
        std::wstring m_metronomeEndpointName{};
        uint8_t m_metronomeGroup{ 0 };
        uint8_t m_metronomeChannel{ 9 };
        uint8_t m_metronomeNote{ 37 };

        bool m_showLauncher{ true };
        double m_barWidth{ DefaultBarWidth };
        uint32_t m_snapTicks{ 240 };
        uint32_t m_launchQuantizeTicks{ 3840 };
        uint32_t m_slotRecordBars{ 4 };
        double m_editorHeight{ 300 };
        ValueDisplay m_valuesAs{ ValueDisplay::Midi2 };
        std::wstring m_lastFolder{};
    };
}
