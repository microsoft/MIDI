// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "MessageListSource.h"
#include "MessagePipeline.h"

namespace winrt::midi2monitor::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow() = default;

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRootSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);

        void OnEndpointSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnGroupSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnChannelSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);

        void OnCaptureButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnEndpointChoiceLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnShowClockToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnShowActiveSenseToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnTimestampFormatChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);

        void OnColumnVisibilityChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMoveColumnUpClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnMoveColumnDownClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAutoHideColumnsChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnShowMessageNamesChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnEditColumnsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnDisplayOptionsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnClearCaptureClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnMessagesContextRequested(xaml::UIElement const& sender, xaml::Input::ContextRequestedEventArgs const& args);
        void OnMessagesContextFlyoutOpening(foundation::IInspectable const& sender, foundation::IInspectable const& args);
        void OnSelectAllClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSelectAllSysEx7Click(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnDeselectAllClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopyUmpWordsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopyMidi1BytesClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopySysExBytesClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnZoomSliderValueChanged(foundation::IInspectable const& sender, controls::Primitives::RangeBaseValueChangedEventArgs const& args);
        void OnZoomPresetClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomOutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomInClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSettingsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnExportClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCommentTextChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);

        // called by the settings dialog
        void ApplyTheme() noexcept;
        void ApplyBackdrop() noexcept;
        void ApplyBackgroundColor() noexcept;
        void ApplyRetention() noexcept;
        void ApplyMessageNameSetting() noexcept;
        void RefreshColumnsAfterReset() noexcept;

        // called before Activate so the window never appears at the wrong size first
        void RestoreWindowPlacement() noexcept;

    private:
        void InitializeWindowChrome() noexcept;
        void ReleaseBackdropControllers() noexcept;
        void UpdateBackdropConfiguration() noexcept;
        void SaveWindowPlacement() noexcept;
        void UpdateTitleBarInsets() noexcept;
        void ApplyTitleBarColors() noexcept;
        void InitializeCollections() noexcept;
        void InitializePipeline() noexcept;
        void InitializeControlsFromSettings() noexcept;
        void InitializeColumns() noexcept;
        void RebuildColumnFlyoutItems() noexcept;
        void PersistColumnLayout() noexcept;
        void InvalidateRowLayout() noexcept;
        void MoveColumn(foundation::IInspectable const& sender, int32_t offset) noexcept;
        void BuildChannelList() noexcept;

        void StartEndpointWatcher() noexcept;
        void StopEndpointWatcher() noexcept;
        void RefreshEndpointList() noexcept;
        void RefreshGroupList() noexcept;
        void UpdateEndpointImage() noexcept;

        void StartMonitoring() noexcept;
        winrt::fire_and_forget StartMonitoringAsync();
        void CompleteMonitoringStart(
            midi2::MidiSession session,
            midi2::MidiEndpointConnection connection,
            winrt::com_ptr<IMidiEndpointConnectionRaw> connectionRaw,
            winrt::hstring endpointName,
            winrt::hstring failureBodyKey) noexcept;
        winrt::fire_and_forget TearDownConnectionAsync(
            midi2::MidiSession session,
            midi2::MidiEndpointConnection connection,
            winrt::com_ptr<IMidiEndpointConnectionRaw> connectionRaw);
        void StopMonitoring(bool addNotice) noexcept;
        void UpdateWindowTitle() noexcept;
        winrt::hstring DescribeSelectedGroup() noexcept;
        winrt::hstring DescribeSelectedChannel() noexcept;
        void UpdateCommandStates() noexcept;
        void SelectAllSysEx7() noexcept;
        void UpdateCaptureButtonLayout() noexcept;
        void UpdateStatusBarLayout() noexcept;
        void ApplyFilterToPipeline() noexcept;
        void ApplyHiddenTraits() noexcept;

        void OnPipelineContentChanged() noexcept;
        void RefreshMessageList() noexcept;
        void UpdateStatusLine() noexcept;

        void ApplyStartupOptions() noexcept;
        void ApplyZoom(uint32_t zoomPercent, bool updateSlider, bool updateChoice) noexcept;
        std::vector<::midi2monitor::MessageRecord> SelectedRecords() noexcept;
        winrt::fire_and_forget ShowCommandLineHelpAsync();
        winrt::fire_and_forget ConfirmClearCaptureAsync();
        winrt::fire_and_forget ShowMessageAsync(winrt::hstring title, winrt::hstring message);
        winrt::fire_and_forget ExportAsync();

        HWND WindowHandle() const noexcept;

        uint8_t SelectedGroupNumber() noexcept;
        uint8_t SelectedChannelNumber() noexcept;
        int32_t FindGroupChoiceIndex(int32_t groupNumber) noexcept;
        winrt::hstring SelectedEndpointDeviceId() noexcept;

        ::midi2monitor::MessagePipeline m_pipeline{};
        winrt::com_ptr<::midi2monitor::MessageListSource> m_listSource{ nullptr };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        midi2::MidiSession m_session{ nullptr };
        midi2::MidiEndpointConnection m_connection{ nullptr };
        winrt::com_ptr<IMidiEndpointConnectionRaw> m_connectionRaw{ nullptr };

        midi2enum::MidiEndpointDeviceWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};

        collections::IObservableVector<midi2monitor::EndpointChoice> m_endpoints{ nullptr };
        // parallel to m_endpoints. Held so group discovery never has to re-resolve an endpoint,
        // which would block the STA UI thread inside the SDK.
        std::vector<midi2enum::MidiEndpointDeviceInformation> m_endpointDevices{};
        collections::IObservableVector<midi2monitor::NamedChoice> m_groups{ nullptr };
        collections::IObservableVector<midi2monitor::NamedChoice> m_channels{ nullptr };
        collections::IObservableVector<midi2monitor::MonitorColumn> m_columns{ nullptr };

        bool m_monitoring{ false };
        bool m_connecting{ false };
        bool m_initialized{ false };
        bool m_startupFilterApplied{ false };
        bool m_startupMonitorHandled{ false };
        bool m_startupMonitorPending{ false };
        bool m_backdropApplied{ false };
        ::midi2monitor::WindowBackdrop m_appliedBackdrop{ ::midi2monitor::WindowBackdrop::Solid };

        // the material is driven through the controllers rather than the XAML SystemBackdrop
        // property, because only the controllers expose TintColor
        winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropConfiguration m_backdropConfiguration{ nullptr };
        winrt::Microsoft::UI::Composition::SystemBackdrops::MicaController m_micaController{ nullptr };
        winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController m_acrylicController{ nullptr };
        winrt::event_token m_activatedToken{};
        bool m_captureButtonStacked{ false };
        bool m_suppressSelectionHandling{ false };
        bool m_suppressZoomHandling{ false };
        winrt::hstring m_monitoredEndpointName{};
    };
}

namespace winrt::midi2monitor::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
