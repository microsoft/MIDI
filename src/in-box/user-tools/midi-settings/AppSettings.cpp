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
        constexpr wchar_t ValueShowToolbarLabels[] = L"ShowToolbarLabels";
        constexpr wchar_t ValueTransportFilter[] = L"TransportFilter";
        constexpr wchar_t ValueDismissedOrphanedCustomizations[] = L"DismissedOrphanedCustomizations";
        constexpr wchar_t ValueClearedPortNames[] = L"ClearedPortNames";

        constexpr wchar_t ValueConfigurationsX[] = L"ConfigurationsWindowX";
        constexpr wchar_t ValueConfigurationsY[] = L"ConfigurationsWindowY";
        constexpr wchar_t ValueConfigurationsWidth[] = L"ConfigurationsWindowWidth";
        constexpr wchar_t ValueConfigurationsHeight[] = L"ConfigurationsWindowHeight";
        constexpr wchar_t ValueConfigurationsMaximized[] = L"ConfigurationsWindowMaximized";
        constexpr wchar_t ValueLastPackageFolder[] = L"LastPackageFolder";
        constexpr wchar_t ValuePendingRestartProcessId[] = L"PendingRestartServiceProcessId";
        constexpr wchar_t ValuePendingRestartKinds[] = L"PendingRestartKinds";
        constexpr wchar_t ValuePendingRestartRunningConfig[] = L"PendingRestartRunningConfig";
        constexpr wchar_t ValueImportedConfigurations[] = L"ImportedConfigurations";
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

        m_showToolbarLabels = ReadDword(ValueShowToolbarLabels, 1) != 0;

        m_transportFilter = ReadString(ValueTransportFilter, std::wstring{});
        m_dismissedOrphanedCustomizations = ReadString(ValueDismissedOrphanedCustomizations, std::wstring{});
        m_clearedPortNameList = ReadString(ValueClearedPortNames, std::wstring{});

        m_configurationsPlacement.X = static_cast<int32_t>(ReadDword(ValueConfigurationsX, 0));
        m_configurationsPlacement.Y = static_cast<int32_t>(ReadDword(ValueConfigurationsY, 0));
        m_configurationsPlacement.Width = static_cast<int32_t>(ReadDword(ValueConfigurationsWidth, 0));
        m_configurationsPlacement.Height = static_cast<int32_t>(ReadDword(ValueConfigurationsHeight, 0));
        m_configurationsPlacement.Maximized = ReadDword(ValueConfigurationsMaximized, 0) != 0;
        m_configurationsPlacement.Valid =
            m_configurationsPlacement.Width >= MinimumWindowWidth &&
            m_configurationsPlacement.Height >= MinimumWindowHeight;

        m_lastPackageFolder = ReadString(ValueLastPackageFolder, std::wstring{});
        m_pendingRestartServiceProcessId = ReadDword(ValuePendingRestartProcessId, 0);
        m_pendingRestartKinds = ReadDword(ValuePendingRestartKinds, 0);
        m_pendingRestartRunningConfig = ReadString(ValuePendingRestartRunningConfig, std::wstring{});
        m_importedConfigurations = ReadString(ValueImportedConfigurations, std::wstring{});
    }

    void AppSettings::ViewMode(EndpointViewMode value) noexcept
    {
        m_viewMode = value;
        WriteDword(ValueViewMode, static_cast<uint32_t>(m_viewMode));
    }

    void AppSettings::ShowToolbarLabels(bool value) noexcept
    {
        m_showToolbarLabels = value;
        WriteDword(ValueShowToolbarLabels, m_showToolbarLabels ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::TransportFilter(std::wstring const& value) noexcept
    {
        m_transportFilter = value;
        WriteString(ValueTransportFilter, m_transportFilter);
    }

    _Use_decl_annotations_
    void AppSettings::DismissedOrphanedCustomizations(std::wstring const& value) noexcept
    {
        m_dismissedOrphanedCustomizations = value;
        WriteString(ValueDismissedOrphanedCustomizations, m_dismissedOrphanedCustomizations);
    }

    _Use_decl_annotations_
    void AppSettings::ClearedPortNameList(std::wstring const& value) noexcept
    {
        m_clearedPortNameList = value;
        WriteString(ValueClearedPortNames, m_clearedPortNameList);
    }

    _Use_decl_annotations_
    void AppSettings::ConfigurationsPlacement(WindowPlacementInfo const& value) noexcept
    {
        m_configurationsPlacement = value;
        m_configurationsPlacement.Valid = true;

        WriteDword(ValueConfigurationsX, static_cast<uint32_t>(value.X));
        WriteDword(ValueConfigurationsY, static_cast<uint32_t>(value.Y));
        WriteDword(ValueConfigurationsWidth, static_cast<uint32_t>(value.Width));
        WriteDword(ValueConfigurationsHeight, static_cast<uint32_t>(value.Height));
        WriteDword(ValueConfigurationsMaximized, value.Maximized ? 1u : 0u);
    }

    _Use_decl_annotations_
    void AppSettings::LastPackageFolder(std::wstring const& value) noexcept
    {
        m_lastPackageFolder = value;
        WriteString(ValueLastPackageFolder, m_lastPackageFolder);
    }

    _Use_decl_annotations_
    void AppSettings::PendingRestart(uint32_t const serviceProcessId, uint32_t const kinds, std::wstring const& runningConfig) noexcept
    {
        m_pendingRestartServiceProcessId = serviceProcessId;
        m_pendingRestartKinds = kinds;
        m_pendingRestartRunningConfig = runningConfig;

        WriteDword(ValuePendingRestartProcessId, serviceProcessId);
        WriteDword(ValuePendingRestartKinds, kinds);
        WriteString(ValuePendingRestartRunningConfig, runningConfig);
    }

    _Use_decl_annotations_
    void AppSettings::ImportedConfigurations(std::wstring const& value) noexcept
    {
        m_importedConfigurations = value;
        WriteString(ValueImportedConfigurations, m_importedConfigurations);
    }
}
