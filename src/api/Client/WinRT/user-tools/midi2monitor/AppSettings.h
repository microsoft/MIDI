// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2monitor
{
    enum class TimestampDisplayFormat : int32_t
    {
        Ticks = 0,
        Microseconds = 1,
        Milliseconds = 2,
        Seconds = 3
    };

    enum class AppTheme : int32_t
    {
        System = 0,
        Light = 1,
        Dark = 2
    };

    enum class WindowBackdrop : int32_t
    {
        Solid = 0,
        Mica = 1,
        Acrylic = 2
    };

    // Restore bounds, so a maximized window still reopens at its previous size.
    struct WindowPlacementInfo
    {
        int32_t X{ 0 };
        int32_t Y{ 0 };
        int32_t Width{ 0 };
        int32_t Height{ 0 };
        bool Maximized{ false };
        bool Valid{ false };
    };

    // Per-user settings, persisted under HKCU so that every instance of the app picks up the
    // same defaults. Reads and writes never throw; on failure the in-memory value is used.
    class AppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        bool ShowClockMessages() const noexcept { return m_showClockMessages; }
        void ShowClockMessages(bool value) noexcept;

        bool ShowActiveSenseMessages() const noexcept { return m_showActiveSenseMessages; }
        void ShowActiveSenseMessages(bool value) noexcept;

        bool AlwaysOnTop() const noexcept { return m_alwaysOnTop; }
        void AlwaysOnTop(bool value) noexcept;

        TimestampDisplayFormat TimestampFormat() const noexcept { return m_timestampFormat; }
        void TimestampFormat(TimestampDisplayFormat value) noexcept;

        AppTheme Theme() const noexcept { return m_theme; }
        void Theme(AppTheme value) noexcept;

        WindowBackdrop Backdrop() const noexcept { return m_backdrop; }
        void Backdrop(WindowBackdrop value) noexcept;

        // 0 means fully opaque; the table is always drawn less transparent than this
        uint32_t BackgroundTransparencyPercent() const noexcept { return m_backgroundTransparencyPercent; }
        void BackgroundTransparencyPercent(uint32_t value) noexcept;

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

        WindowPlacementInfo const& WindowPlacement() const noexcept { return m_windowPlacement; }
        void WindowPlacement(WindowPlacementInfo const& value) noexcept;

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

        static constexpr uint32_t MaximumTransparencyPercent = 90;

    private:
        AppSettings() = default;

        void WriteDword(std::wstring_view valueName, uint32_t value) const noexcept;
        void WriteString(std::wstring_view valueName, std::wstring const& value) const noexcept;
        uint32_t ReadDword(std::wstring_view valueName, uint32_t defaultValue) const noexcept;
        std::wstring ReadString(std::wstring_view valueName, std::wstring const& defaultValue) const noexcept;

        bool m_showClockMessages{ false };
        bool m_showActiveSenseMessages{ false };
        bool m_alwaysOnTop{ false };
        bool m_showMessageNameChiclets{ true };
        bool m_autoHideColumnsWhenNarrow{ true };
        TimestampDisplayFormat m_timestampFormat{ TimestampDisplayFormat::Ticks };
        AppTheme m_theme{ AppTheme::System };
        WindowBackdrop m_backdrop{ WindowBackdrop::Solid };
        uint32_t m_backgroundTransparencyPercent{ 0 };
        uint32_t m_retainedMessageCount{ DefaultRetainedMessageCount };
        uint32_t m_tableZoomPercent{ DefaultZoomPercent };
        std::wstring m_columnLayout{};
        WindowPlacementInfo m_windowPlacement{};
    };
}
