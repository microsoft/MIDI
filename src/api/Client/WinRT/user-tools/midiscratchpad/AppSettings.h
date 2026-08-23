// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midiscratchpad
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    enum class ScratchPadMode : int32_t
    {
        // hex bytes, converted to UMP for the group the customer picked
        Midi1Bytes = 0,

        // 32 bit UMP words, sent exactly as typed
        UmpWords = 1
    };

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        ScratchPadMode Mode() const noexcept { return m_mode; }
        void Mode(ScratchPadMode value) noexcept;

        bool AllowRunningStatus() const noexcept { return m_allowRunningStatus; }
        void AllowRunningStatus(bool value) noexcept;

        bool ShowSendPreview() const noexcept { return m_showSendPreview; }
        void ShowSendPreview(bool value) noexcept;

        uint32_t EditorFontPercent() const noexcept { return m_editorFontPercent; }
        void EditorFontPercent(uint32_t value) noexcept;

        static constexpr uint32_t MinimumEditorFontPercent = 75;
        static constexpr uint32_t MaximumEditorFontPercent = 200;
        static constexpr uint32_t DefaultEditorFontPercent = 100;

        static constexpr double BaseEditorFontSize = 15.0;

    private:
        AppSettings() noexcept;

        ScratchPadMode m_mode{ ScratchPadMode::Midi1Bytes };
        bool m_allowRunningStatus{ false };
        bool m_showSendPreview{ false };
        uint32_t m_editorFontPercent{ DefaultEditorFontPercent };
    };
}
