// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midinetworksetup
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midinetworksetup)";

        constexpr wchar_t ValueRefreshIntervalSeconds[] = L"RefreshIntervalSeconds";
        constexpr wchar_t ValueSelectedPageIndex[] = L"SelectedPageIndex";
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

        m_refreshIntervalSeconds = std::clamp(
            ReadDword(ValueRefreshIntervalSeconds, DefaultRefreshIntervalSeconds),
            MinimumRefreshIntervalSeconds,
            MaximumRefreshIntervalSeconds);

        auto const page = ReadDword(ValueSelectedPageIndex, PageIndexRemoteHosts);
        m_selectedPageIndex = page > PageIndexTransportSettings ? PageIndexRemoteHosts : page;
    }

    void AppSettings::RefreshIntervalSeconds(uint32_t value) noexcept
    {
        m_refreshIntervalSeconds = std::clamp(value, MinimumRefreshIntervalSeconds, MaximumRefreshIntervalSeconds);
        WriteDword(ValueRefreshIntervalSeconds, m_refreshIntervalSeconds);
    }

    void AppSettings::SelectedPageIndex(uint32_t value) noexcept
    {
        m_selectedPageIndex = value > PageIndexTransportSettings ? PageIndexRemoteHosts : value;
        WriteDword(ValueSelectedPageIndex, m_selectedPageIndex);
    }
}
