// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

namespace midisettings
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    enum class EndpointViewMode : uint32_t
    {
        Cards = 0,
        List = 1
    };

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        EndpointViewMode ViewMode() const noexcept { return m_viewMode; }
        void ViewMode(EndpointViewMode value) noexcept;

        // Whether the toolbar buttons carry their names. Off gives an icon-only toolbar; the
        // toolbar also drops the labels on its own when the window is too narrow for them.
        bool ShowToolbarLabels() const noexcept { return m_showToolbarLabels; }
        void ShowToolbarLabels(bool value) noexcept;

        // Transport code the endpoint list is filtered to, or empty for all of them.
        std::wstring TransportFilter() const noexcept { return m_transportFilter; }
        void TransportFilter(std::wstring const& value) noexcept;

        // Stored ids of orphaned customizations the customer has already been told about. A
        // customer who chooses to live with an orphan should not be asked again every launch.
        std::wstring DismissedOrphanedCustomizations() const noexcept { return m_dismissedOrphanedCustomizations; }
        void DismissedOrphanedCustomizations(std::wstring const& value) noexcept;

        // MIDI 1.0 port names this app cleared, which the service may still show until it restarts.
        std::wstring ClearedPortNameList() const noexcept { return m_clearedPortNameList; }
        void ClearedPortNameList(_In_ std::wstring const& value) noexcept;

        // The Configurations window keeps its own placement, so it doesn't move the main window.
        WindowPlacementInfo const& ConfigurationsPlacement() const noexcept { return m_configurationsPlacement; }
        void ConfigurationsPlacement(_In_ WindowPlacementInfo const& value) noexcept;

        // The folder a package was last saved to or opened from.
        std::wstring LastPackageFolder() const noexcept { return m_lastPackageFolder; }
        void LastPackageFolder(_In_ std::wstring const& value) noexcept;

        // A change the MIDI service only picks up when it restarts, and the service process it was
        // made under. A different process means the service has restarted since, and the reminder
        // is stale. Kept here, so Reopen as administrator still shows it.
        uint32_t PendingRestartServiceProcessId() const noexcept { return m_pendingRestartServiceProcessId; }
        uint32_t PendingRestartKinds() const noexcept { return m_pendingRestartKinds; }
        std::wstring PendingRestartRunningConfig() const noexcept { return m_pendingRestartRunningConfig; }
        void PendingRestart(_In_ uint32_t serviceProcessId, _In_ uint32_t kinds, _In_ std::wstring const& runningConfig) noexcept;

        // Configurations imported as new ones, as file|computer|time entries, so the list can say
        // where each came from.
        std::wstring ImportedConfigurations() const noexcept { return m_importedConfigurations; }
        void ImportedConfigurations(_In_ std::wstring const& value) noexcept;

    private:
        AppSettings() noexcept;

        EndpointViewMode m_viewMode{ EndpointViewMode::Cards };
        bool m_showToolbarLabels{ true };
        std::wstring m_transportFilter{};
        std::wstring m_dismissedOrphanedCustomizations{};
        std::wstring m_clearedPortNameList{};
        WindowPlacementInfo m_configurationsPlacement{};
        std::wstring m_lastPackageFolder{};
        uint32_t m_pendingRestartServiceProcessId{ 0 };
        uint32_t m_pendingRestartKinds{ 0 };
        std::wstring m_pendingRestartRunningConfig{};
        std::wstring m_importedConfigurations{};
    };
}
