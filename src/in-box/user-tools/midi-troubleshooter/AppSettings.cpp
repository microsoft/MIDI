// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace miditroubleshooter
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\miditroubleshooter)";

        constexpr wchar_t ValueRefreshIntervalSeconds[] = L"RefreshIntervalSeconds";
        constexpr wchar_t ValueSelectedPageIndex[] = L"SelectedPageIndex";
        constexpr wchar_t ValueLastCaptureFolder[] = L"LastCaptureFolder";
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

        auto const page = ReadDword(ValueSelectedPageIndex, PageIndexApiMode);
        m_selectedPageIndex = page > PageIndexMaximum ? PageIndexApiMode : page;

        m_lastCaptureFolder = ReadString(ValueLastCaptureFolder, std::wstring{});
    }

    void AppSettings::RefreshIntervalSeconds(uint32_t value) noexcept
    {
        m_refreshIntervalSeconds = std::clamp(value, MinimumRefreshIntervalSeconds, MaximumRefreshIntervalSeconds);
        WriteDword(ValueRefreshIntervalSeconds, m_refreshIntervalSeconds);
    }

    void AppSettings::SelectedPageIndex(uint32_t value) noexcept
    {
        m_selectedPageIndex = value > PageIndexMaximum ? PageIndexApiMode : value;
        WriteDword(ValueSelectedPageIndex, m_selectedPageIndex);
    }

    _Use_decl_annotations_
    void AppSettings::LastCaptureFolder(std::wstring const& value) noexcept
    {
        m_lastCaptureFolder = value;
        WriteString(ValueLastCaptureFolder, m_lastCaptureFolder);
    }
}
