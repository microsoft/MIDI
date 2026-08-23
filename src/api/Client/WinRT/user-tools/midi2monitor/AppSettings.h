// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midi2monitor
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so existing call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    enum class TimestampDisplayFormat : int32_t
    {
        Ticks = 0,
        Microseconds = 1,
        Milliseconds = 2,
        Seconds = 3
    };

    // Per-user settings, persisted under HKCU so that every instance of the app picks up the
    // same defaults. Reads and writes never throw; on failure the in-memory value is used.
    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        bool ShowClockMessages() const noexcept { return m_showClockMessages; }
        void ShowClockMessages(bool value) noexcept;

        bool ShowActiveSenseMessages() const noexcept { return m_showActiveSenseMessages; }
        void ShowActiveSenseMessages(bool value) noexcept;

        TimestampDisplayFormat TimestampFormat() const noexcept { return m_timestampFormat; }
        void TimestampFormat(TimestampDisplayFormat value) noexcept;

        uint32_t RetainedMessageCount() const noexcept { return m_retainedMessageCount; }
        void RetainedMessageCount(uint32_t value) noexcept;

        bool ShowMessageNameChiclets() const noexcept { return m_showMessageNameChiclets; }
        void ShowMessageNameChiclets(bool value) noexcept;

        bool AutoHideColumnsWhenNarrow() const noexcept { return m_autoHideColumnsWhenNarrow; }
        void AutoHideColumnsWhenNarrow(bool value) noexcept;

        // comma separated list of column ids, in display order, with a '-' prefix for hidden
        std::wstring ColumnLayout() const noexcept { return m_columnLayout; }
        void ColumnLayout(std::wstring const& value) noexcept;

        uint32_t TableZoomPercent() const noexcept { return m_tableZoomPercent; }
        void TableZoomPercent(uint32_t value) noexcept;

        static constexpr uint32_t MinimumRetainedMessageCount = 100;
        static constexpr uint32_t MaximumRetainedMessageCount = 500000;
        static constexpr uint32_t DefaultRetainedMessageCount = 10000;

        static constexpr uint32_t MinimumZoomPercent = 50;
        static constexpr uint32_t MaximumZoomPercent = 200;
        static constexpr uint32_t DefaultZoomPercent = 100;

        // font size the table uses at 100%
        static constexpr double BaseRowFontSize = 12.0;
        static constexpr double BaseChicletFontSize = 11.0;

        // comment gutter button at 100%; it only ever scales down, since the gutter is fixed width
        static constexpr double BaseCommentButtonSize = 22.0;
        static constexpr double BaseCommentGlyphSize = 12.0;

        static constexpr int32_t MinimumWindowWidth = 640;
        static constexpr int32_t MinimumWindowHeight = 400;

    private:
        AppSettings() noexcept;

        bool m_showClockMessages{ false };
        bool m_showActiveSenseMessages{ false };
        bool m_showMessageNameChiclets{ true };
        bool m_autoHideColumnsWhenNarrow{ true };
        TimestampDisplayFormat m_timestampFormat{ TimestampDisplayFormat::Ticks };
        uint32_t m_retainedMessageCount{ DefaultRetainedMessageCount };
        uint32_t m_tableZoomPercent{ DefaultZoomPercent };
        std::wstring m_columnLayout{};
    };
}
