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
        constexpr wchar_t ValueAskBeforeClear[] = L"AskBeforeClear";
        constexpr wchar_t ValueLastEndpoint[] = L"LastEndpointDeviceId";
        constexpr wchar_t ValueLastGroup[] = L"LastGroupNumber";
        constexpr wchar_t ValueLastChannel[] = L"LastChannelNumber";
        constexpr wchar_t ValueAdvancedMode[] = L"AdvancedMode";
        constexpr wchar_t ValueAdvancedSources[] = L"AdvancedSources";
        constexpr wchar_t ValueAdvancedFilter[] = L"AdvancedFilter";
        constexpr wchar_t ValueFoldRepeats[] = L"FoldRepeats";
        constexpr wchar_t ValueShowEndpointActivity[] = L"ShowEndpointActivity";
        constexpr wchar_t ValueOnlySendingEndpoints[] = L"OnlySendingEndpoints";
        constexpr wchar_t ValueSourcesPaneOpen[] = L"SourcesPaneOpen";

        // Generous: 256 endpoints of a few hundred characters each. A longer value isn't ours.
        constexpr size_t MaximumSourcesLength = 256 * 1024;
        constexpr size_t MaximumFilterLength = 16 * 1024;

        // Endpoint ids are a few hundred characters at most. A longer value read back isn't one.
        constexpr size_t MaximumEndpointIdLength = 1024;
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

        m_askBeforeClear = ReadDword(ValueAskBeforeClear, 1) != 0;

        m_lastEndpointDeviceId = ReadString(ValueLastEndpoint, L"");

        if (m_lastEndpointDeviceId.size() > MaximumEndpointIdLength)
        {
            m_lastEndpointDeviceId.clear();
        }

        auto const group = ReadDword(ValueLastGroup, 0);
        auto const channel = ReadDword(ValueLastChannel, 0);

        m_lastGroupNumber = static_cast<uint8_t>(group <= 16 ? group : 0);
        m_lastChannelNumber = static_cast<uint8_t>(channel <= 16 ? channel : 0);

        m_advancedMode = ReadDword(ValueAdvancedMode, 0) != 0;
        m_foldRepeats = ReadDword(ValueFoldRepeats, 0) != 0;
        m_showEndpointActivity = ReadDword(ValueShowEndpointActivity, 1) != 0;
        m_onlySendingEndpoints = ReadDword(ValueOnlySendingEndpoints, 0) != 0;
        m_sourcesPaneOpen = ReadDword(ValueSourcesPaneOpen, 1) != 0;

        m_advancedSources = ReadString(ValueAdvancedSources, L"");

        if (m_advancedSources.size() > MaximumSourcesLength)
        {
            m_advancedSources.clear();
        }

        m_advancedFilter = ReadString(ValueAdvancedFilter, L"");

        if (m_advancedFilter.size() > MaximumFilterLength)
        {
            m_advancedFilter.clear();
        }
    }

    _Use_decl_annotations_
    void AppSettings::AdvancedMode(bool value) noexcept
    {
        m_advancedMode = value;
        WriteDword(ValueAdvancedMode, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::AdvancedSources(std::wstring const& value) noexcept
    {
        try
        {
            if (value == m_advancedSources)
            {
                return;
            }

            m_advancedSources = value;
            WriteString(ValueAdvancedSources, value);
        }
        catch (...)
        {
            // kept in memory only; written the next time it changes
        }
    }

    _Use_decl_annotations_
    void AppSettings::AdvancedFilter(std::wstring const& value) noexcept
    {
        try
        {
            if (value == m_advancedFilter)
            {
                return;
            }

            m_advancedFilter = value;
            WriteString(ValueAdvancedFilter, value);
        }
        catch (...)
        {
            // kept in memory only; written the next time it changes
        }
    }

    _Use_decl_annotations_
    void AppSettings::FoldRepeats(bool value) noexcept
    {
        m_foldRepeats = value;
        WriteDword(ValueFoldRepeats, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::ShowEndpointActivity(bool value) noexcept
    {
        m_showEndpointActivity = value;
        WriteDword(ValueShowEndpointActivity, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::OnlySendingEndpoints(bool value) noexcept
    {
        m_onlySendingEndpoints = value;
        WriteDword(ValueOnlySendingEndpoints, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::SourcesPaneOpen(bool value) noexcept
    {
        m_sourcesPaneOpen = value;
        WriteDword(ValueSourcesPaneOpen, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::AskBeforeClear(bool value) noexcept
    {
        m_askBeforeClear = value;
        WriteDword(ValueAskBeforeClear, value ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::LastSimpleSelection(std::wstring const& endpointDeviceId, uint8_t groupNumber, uint8_t channelNumber) noexcept
    {
        try
        {
            auto const group = static_cast<uint8_t>(groupNumber <= 16 ? groupNumber : 0);
            auto const channel = static_cast<uint8_t>(channelNumber <= 16 ? channelNumber : 0);

            if (endpointDeviceId != m_lastEndpointDeviceId)
            {
                m_lastEndpointDeviceId = endpointDeviceId;
                WriteString(ValueLastEndpoint, endpointDeviceId);
            }

            if (group != m_lastGroupNumber)
            {
                m_lastGroupNumber = group;
                WriteDword(ValueLastGroup, group);
            }

            if (channel != m_lastChannelNumber)
            {
                m_lastChannelNumber = channel;
                WriteDword(ValueLastChannel, channel);
            }
        }
        catch (...)
        {
            // the in-memory values stay as they were; the next pick tries again
        }
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
