// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midi2monitor
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midi2monitor)";

        constexpr wchar_t ValueShowClock[] = L"ShowClockMessages";
        constexpr wchar_t ValueShowActiveSense[] = L"ShowActiveSenseMessages";
        constexpr wchar_t ValueTimestampFormat[] = L"TimestampFormat";
        constexpr wchar_t ValueRetainedMessageCount[] = L"RetainedMessageCount";
        constexpr wchar_t ValueShowChiclets[] = L"ShowMessageNameChiclets";
        constexpr wchar_t ValueAutoHideColumns[] = L"AutoHideColumnsWhenNarrow";
        constexpr wchar_t ValueColumnLayout[] = L"ColumnLayout";
        constexpr wchar_t ValueTableZoomPercent[] = L"TableZoomPercent";
    }

    AppSettings::AppSettings() noexcept :
        midiapp::MidiAppSettings(SettingsKeyPath)
    {
    }

    AppSettings& AppSettings::Current() noexcept
    {
        static AppSettings instance{};
        return instance;
    }

    void AppSettings::Load() noexcept
    {
        LoadShared();

        m_showClockMessages = ReadDword(ValueShowClock, 0) != 0;
        m_showActiveSenseMessages = ReadDword(ValueShowActiveSense, 0) != 0;
        m_showMessageNameChiclets = ReadDword(ValueShowChiclets, 1) != 0;
        m_autoHideColumnsWhenNarrow = ReadDword(ValueAutoHideColumns, 1) != 0;

        auto const format = ReadDword(ValueTimestampFormat, static_cast<uint32_t>(TimestampDisplayFormat::Ticks));
        m_timestampFormat = (format <= static_cast<uint32_t>(TimestampDisplayFormat::Seconds))
            ? static_cast<TimestampDisplayFormat>(format)
            : TimestampDisplayFormat::Ticks;

        auto const retained = ReadDword(ValueRetainedMessageCount, DefaultRetainedMessageCount);
        m_retainedMessageCount = std::clamp(retained, MinimumRetainedMessageCount, MaximumRetainedMessageCount);

        m_columnLayout = ReadString(ValueColumnLayout, L"");

        auto const zoom = ReadDword(ValueTableZoomPercent, DefaultZoomPercent);
        m_tableZoomPercent = std::clamp(zoom, MinimumZoomPercent, MaximumZoomPercent);
    }

    void AppSettings::TableZoomPercent(uint32_t value) noexcept
    {
        m_tableZoomPercent = std::clamp(value, MinimumZoomPercent, MaximumZoomPercent);
        WriteDword(ValueTableZoomPercent, m_tableZoomPercent);
    }

    void AppSettings::ShowClockMessages(bool value) noexcept
    {
        m_showClockMessages = value;
        WriteDword(ValueShowClock, value ? 1u : 0u);
    }

    void AppSettings::ShowActiveSenseMessages(bool value) noexcept
    {
        m_showActiveSenseMessages = value;
        WriteDword(ValueShowActiveSense, value ? 1u : 0u);
    }

    void AppSettings::ShowMessageNameChiclets(bool value) noexcept
    {
        m_showMessageNameChiclets = value;
        WriteDword(ValueShowChiclets, value ? 1u : 0u);
    }

    void AppSettings::AutoHideColumnsWhenNarrow(bool value) noexcept
    {
        m_autoHideColumnsWhenNarrow = value;
        WriteDword(ValueAutoHideColumns, value ? 1u : 0u);
    }

    void AppSettings::TimestampFormat(TimestampDisplayFormat value) noexcept
    {
        m_timestampFormat = value;
        WriteDword(ValueTimestampFormat, static_cast<uint32_t>(value));
    }

    void AppSettings::RetainedMessageCount(uint32_t value) noexcept
    {
        m_retainedMessageCount = std::clamp(value, MinimumRetainedMessageCount, MaximumRetainedMessageCount);
        WriteDword(ValueRetainedMessageCount, m_retainedMessageCount);
    }

    void AppSettings::ColumnLayout(std::wstring const& value) noexcept
    {
        m_columnLayout = value;
        WriteString(ValueColumnLayout, value);
    }
}
