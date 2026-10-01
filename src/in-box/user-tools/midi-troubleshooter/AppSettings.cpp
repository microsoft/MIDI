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

        constexpr wchar_t ValueReportViewerX[] = L"ReportViewerWindowX";
        constexpr wchar_t ValueReportViewerY[] = L"ReportViewerWindowY";
        constexpr wchar_t ValueReportViewerWidth[] = L"ReportViewerWindowWidth";
        constexpr wchar_t ValueReportViewerHeight[] = L"ReportViewerWindowHeight";
        constexpr wchar_t ValueReportViewerMaximized[] = L"ReportViewerWindowMaximized";
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

        m_reportViewerPlacement.X = static_cast<int32_t>(ReadDword(ValueReportViewerX, 0));
        m_reportViewerPlacement.Y = static_cast<int32_t>(ReadDword(ValueReportViewerY, 0));
        m_reportViewerPlacement.Width = static_cast<int32_t>(ReadDword(ValueReportViewerWidth, 0));
        m_reportViewerPlacement.Height = static_cast<int32_t>(ReadDword(ValueReportViewerHeight, 0));
        m_reportViewerPlacement.Maximized = ReadDword(ValueReportViewerMaximized, 0) != 0;

        m_reportViewerPlacement.Valid =
            m_reportViewerPlacement.Width >= MinimumWindowWidth &&
            m_reportViewerPlacement.Height >= MinimumWindowHeight;
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

    _Use_decl_annotations_
    void AppSettings::ReportViewerPlacement(WindowPlacementInfo const& value) noexcept
    {
        m_reportViewerPlacement = value;
        m_reportViewerPlacement.Valid = true;

        WriteDword(ValueReportViewerX, static_cast<uint32_t>(value.X));
        WriteDword(ValueReportViewerY, static_cast<uint32_t>(value.Y));
        WriteDword(ValueReportViewerWidth, static_cast<uint32_t>(value.Width));
        WriteDword(ValueReportViewerHeight, static_cast<uint32_t>(value.Height));
        WriteDword(ValueReportViewerMaximized, value.Maximized ? 1u : 0u);
    }
}
