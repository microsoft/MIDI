// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midisettings
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midisettings)";

        constexpr wchar_t ValueViewMode[] = L"EndpointViewMode";
        constexpr wchar_t ValueTransportFilter[] = L"TransportFilter";
        constexpr wchar_t ValueLastConfigCopyFolder[] = L"LastConfigCopyFolder";
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

        m_viewMode = ReadDword(ValueViewMode, static_cast<uint32_t>(EndpointViewMode::Cards)) ==
            static_cast<uint32_t>(EndpointViewMode::List) ? EndpointViewMode::List : EndpointViewMode::Cards;

        m_transportFilter = ReadString(ValueTransportFilter, std::wstring{});
        m_lastConfigCopyFolder = ReadString(ValueLastConfigCopyFolder, std::wstring{});
    }

    void AppSettings::ViewMode(EndpointViewMode value) noexcept
    {
        m_viewMode = value;
        WriteDword(ValueViewMode, static_cast<uint32_t>(m_viewMode));
    }

    _Use_decl_annotations_
    void AppSettings::TransportFilter(std::wstring const& value) noexcept
    {
        m_transportFilter = value;
        WriteString(ValueTransportFilter, m_transportFilter);
    }

    _Use_decl_annotations_
    void AppSettings::LastConfigCopyFolder(std::wstring const& value) noexcept
    {
        m_lastConfigCopyFolder = value;
        WriteString(ValueLastConfigCopyFolder, m_lastConfigCopyFolder);
    }
}
