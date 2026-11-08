// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "OverviewPanel.g.h"

namespace winrt::miditroubleshooter::implementation
{
    // Everything the Overview shows, gathered by the main window from the checks the other
    // pages run. A section whose Known flag is false shows as "Checking..." until its check
    // finishes.
    struct OverviewState
    {
        struct Service
        {
            bool Known{ false };
            bool Installed{ true };
            bool Running{ false };
            bool Disabled{ false };
            bool StartsWithWindows{ false };
            winrt::hstring StateText{};
            winrt::hstring StartedText{};
        } MidiService{};

        struct Transports
        {
            bool Known{ false };
            bool ServiceRunning{ false };
            uint32_t Total{ 0 };
            uint32_t InWindows{ 0 };
            uint32_t Preview{ 0 };
            uint32_t TurnedOff{ 0 };
            uint32_t Problems{ 0 };
            bool AwaitingRestart{ false };
        } TransportCounts{};

        struct Registry
        {
            bool Known{ false };
            bool LegacyMode{ false };
            uint32_t Checked{ 0 };
            uint32_t ToRepair{ 0 };
            uint32_t ToReview{ 0 };
            uint32_t MmcssErrors{ 0 };
            uint32_t MmcssWarnings{ 0 };
        } RegistryCounts{};

        struct Devices
        {
            bool Known{ false };
            uint32_t Total{ 0 };
            uint32_t NewDriver{ 0 };
            uint32_t ClassicDriver{ 0 };
            uint32_t OtherDriver{ 0 };
            uint32_t WithProblems{ 0 };
            bool KorgUsbInstalled{ false };
            bool KorgBleInstalled{ false };
        } DeviceCounts{};

        struct Sessions
        {
            bool Known{ false };
            bool ServiceRunning{ false };
            uint32_t Apps{ 0 };
            uint32_t Connections{ 0 };
            winrt::hstring Names{};
        } SessionCounts{};

        struct Crashes
        {
            bool Known{ false };
            uint32_t RecentCount{ 0 };
            winrt::hstring LatestText{};
        } CrashCounts{};

        winrt::hstring ApiModeName{};
        winrt::hstring ApiModeDetail{};

        // Legacy MIDI mode doesn't use the MIDI service, so its cards are shown as not used
        bool LegacyApiMode{ false };
    };

    struct OverviewPanel : OverviewPanelT<OverviewPanel>
    {
        OverviewPanel();

        // What the page needs from the window it's in.
        struct Host
        {
            std::function<void(winrt::hstring const&)> Navigate{};
            std::function<void()> Refresh{};
            std::function<void()> RunDiagnostics{};
            std::function<void()> CaptureLog{};
        };

        void Initialize(_In_ Host host) noexcept;
        void Apply(_In_ OverviewState const& state) noexcept;

        void OnRefreshClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRunDiagnosticsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnCaptureClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnTileLinkClick(_In_ miditroubleshooter::OverviewTile const& sender, _In_ foundation::IInspectable const& args);
        void OnTilesSizeChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::SizeChangedEventArgs const& args);

    private:
        struct Issue
        {
            int32_t Severity{ 1 };
            winrt::hstring PillText{};
            winrt::hstring Message{};
            winrt::hstring LinkText{};
            winrt::hstring Tag{};
        };

        void ApplyService(_In_ OverviewState const& state, _Inout_ std::vector<Issue>& issues) noexcept;
        void ApplyTransports(_In_ OverviewState const& state, _Inout_ std::vector<Issue>& issues) noexcept;
        void ApplyRegistry(_In_ OverviewState const& state, _Inout_ std::vector<Issue>& issues) noexcept;
        void ApplyDevices(_In_ OverviewState const& state, _Inout_ std::vector<Issue>& issues) noexcept;
        void ApplySessions(_In_ OverviewState const& state) noexcept;
        void ApplyCrashes(_In_ OverviewState const& state, _Inout_ std::vector<Issue>& issues) noexcept;
        void ApplyApiModeAndChecks(_In_ OverviewState const& state) noexcept;
        void ApplyIssues(_In_ std::vector<Issue> const& issues, _In_ bool allKnown, _In_ bool legacyMode) noexcept;

        // puts the cards in as many columns as fit
        void LayoutTiles(_In_ uint32_t columns) noexcept;

        uint32_t m_tileColumns{ 0 };

        // Legacy MIDI mode changes the order of the cards
        bool m_legacy{ false };
        bool m_tilesLaidOutForLegacy{ false };

        Host m_host{};

        // what the issue list last showed, so an unchanged list is left alone
        std::wstring m_issuesSignature{};
    };
}

namespace winrt::miditroubleshooter::factory_implementation
{
    struct OverviewPanel : OverviewPanelT<OverviewPanel, implementation::OverviewPanel>
    {
    };
}
