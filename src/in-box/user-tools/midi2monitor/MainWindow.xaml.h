// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "AdvancedConnections.h"
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
        void OnClearAcceleratorInvoked(_In_ xaml::Input::KeyboardAccelerator const& sender, _In_ xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnCopyAcceleratorInvoked(_In_ xaml::Input::KeyboardAccelerator const& sender, _In_ xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);

        // advanced mode (MainWindow.Advanced.cpp, MainWindow.Sources.cpp, MainWindow.Filters.cpp)
        void OnAdvancedModeToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnAdvancedModeAcceleratorInvoked(_In_ xaml::Input::KeyboardAccelerator const& sender, _In_ xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args);
        void OnAddToSavedSourcesClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSwitchToSavedSourcesClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSourcesPaneToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnFilterButtonClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSourcesSearchChanged(_In_ foundation::IInspectable const& sender, _In_ controls::TextChangedEventArgs const& args);
        void OnOnlySendingToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSourcesSplitterPressed(_In_ foundation::IInspectable const& sender, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnSourcesSplitterMoved(_In_ foundation::IInspectable const& sender, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnSourcesSplitterReleased(_In_ foundation::IInspectable const& sender, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnSourcesSplitterEntered(_In_ foundation::IInspectable const& sender, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnSourcesSplitterExited(_In_ foundation::IInspectable const& sender, _In_ xaml::Input::PointerRoutedEventArgs const& args);

        // called by the settings dialog
        void ApplyActivitySetting() noexcept;

        // Content dialogs take the window's color, a shade deeper where the buttons are, the way
        // the context menus take it. Called when the theme or the window color changes.
        void ColorDialogs() noexcept;

        void OnMessagesContextRequested(xaml::UIElement const& sender, xaml::Input::ContextRequestedEventArgs const& args);
        void OnMessagesContextFlyoutOpening(foundation::IInspectable const& sender, foundation::IInspectable const& args);
        void OnSelectAllClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSelectAllSysEx7Click(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnDeselectAllClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopyUmpWordsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopyBinaryDataClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnCopyMidi1BytesClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCopySysExBytesClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnZoomSliderValueChanged(foundation::IInspectable const& sender, controls::Primitives::RangeBaseValueChangedEventArgs const& args);
        void OnZoomPresetClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomOutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomInClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSettingsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnExportClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCommentTextChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnCommentButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

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
        void SaveWindowPlacement() noexcept;
        void UpdateTitleBarInsets() noexcept;
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

        // Keeps what simple mode is watching for the next start. A window opened by another app
        // with an endpoint on its command line doesn't, so that app never replaces the choice.
        void SaveSimpleSelection() noexcept;
        bool ShouldRememberSelection() noexcept;

        // ---------------- advanced mode ----------------

        // One endpoint in the Sources list, and the controls that show it.
        struct SourceRowGroup
        {
            uint8_t GroupNumber{ 0 };
            controls::Grid Row{ nullptr };
            controls::CheckBox Check{ nullptr };
            xaml::Shapes::Ellipse Light{ nullptr };
            xaml::Shapes::Rectangle Swatch{ nullptr };
        };

        struct SourceRow
        {
            std::wstring EndpointDeviceId{};
            winrt::hstring Name{};
            winrt::hstring ImagePath{};
            winrt::hstring TransportCode{};
            uint16_t Slot{ 0xFFFF };
            std::array<bool, 16> Declared{};
            bool Expanded{ false };
            bool Sending{ false };

            controls::Grid Row{ nullptr };
            controls::CheckBox Check{ nullptr };
            controls::Button Expander{ nullptr };
            xaml::Shapes::Ellipse Light{ nullptr };
            std::vector<SourceRowGroup> Groups{};
        };

        // The groups being listened to, by lowercase endpoint id. Endpoints that aren't here now
        // keep their entry, so a device unplugged for a while comes back checked.
        struct SourceChoice
        {
            std::wstring EndpointDeviceId{};
            uint32_t GroupMask{ 0 };
        };

        void InitializeAdvancedMode() noexcept;
        void ApplyMode(_In_ bool advanced, _In_ bool remember) noexcept;
        void UpdateAdvancedLayout() noexcept;
        void LoadSavedSources() noexcept;
        void PersistSources() noexcept;
        void UseSessionSources(_In_ std::wstring const& endpointDeviceId, _In_ std::optional<uint8_t> groupNumber) noexcept;
        uint32_t AllGroupsMask(_In_ SourceRow const& row) const noexcept;
        uint32_t GroupMaskFor(_In_ std::wstring const& endpointDeviceId) const noexcept;
        void SetGroupMask(_In_ std::wstring const& endpointDeviceId, _In_ uint32_t mask) noexcept;
        size_t CheckedGroupCount() const noexcept;

        void RefreshSourcesList() noexcept;
        void BuildSourceRow(_Inout_ SourceRow& row) noexcept;
        void UpdateSourceRowStates() noexcept;
        void ApplySourcesSearchAndActivity() noexcept;
        void OnSourceCheckChanged(_In_ std::wstring const& endpointDeviceId, _In_ uint8_t groupNumber, _In_ bool isChecked) noexcept;
        void ToggleSourceExpanded(_In_ std::wstring const& endpointDeviceId) noexcept;
        void PushSlotMasks() noexcept;
        void UpdateDesiredConnections() noexcept;
        void UpdateSourcesSummary() noexcept;
        void RefreshActivityLights() noexcept;

        void StartAdvancedListening() noexcept;
        void StopAdvancedListening(_In_ bool addNotice) noexcept;

        void LoadMessageFilter() noexcept;
        void ApplyMessageFilter() noexcept;
        void BuildFilterFlyout() noexcept;
        void RebuildChips() noexcept;

        void OnPipelineContentChanged() noexcept;
        void RefreshMessageList() noexcept;
        void UpdateStatusLine() noexcept;

        // The list follows new messages only while the customer is already at the end. A full
        // refresh cannot rely on ItemsUpdatingScrollMode, so the position is restored by hand.
        controls::ScrollViewer MessageListScrollViewer() noexcept;
        bool IsMessageListAtEnd() noexcept;
        void ScrollMessageListToEnd() noexcept;

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

        controls::ScrollViewer m_messagesScrollViewer{ nullptr };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        midi2::MidiSession m_session{ nullptr };
        midi2::MidiEndpointConnection m_connection{ nullptr };
        winrt::com_ptr<IMidiEndpointConnectionRaw> m_connectionRaw{ nullptr };

        midi2enum::MidiEndpointDeviceWatcher m_watcher{ nullptr };
        winrt::event_token m_watcherAddedToken{};
        winrt::event_token m_watcherRemovedToken{};
        winrt::event_token m_watcherUpdatedToken{};

        collections::IObservableVector<appshared::EndpointChoice> m_endpoints{ nullptr };
        // parallel to m_endpoints. Held so group discovery never has to re-resolve an endpoint,
        // which would block the STA UI thread inside the SDK.
        std::vector<midi2enum::MidiEndpointDeviceInformation> m_endpointDevices{};
        collections::IObservableVector<appshared::NamedChoice> m_groups{ nullptr };
        collections::IObservableVector<appshared::NamedChoice> m_channels{ nullptr };
        collections::IObservableVector<midi2monitor::MonitorColumn> m_columns{ nullptr };

        bool m_monitoring{ false };
        bool m_connecting{ false };
        bool m_initialized{ false };
        bool m_startupFilterApplied{ false };
        bool m_startupMonitorHandled{ false };
        bool m_startupMonitorPending{ false };

        midiapp::WindowChrome m_chrome{};
        bool m_captureButtonStacked{ false };
        bool m_suppressSelectionHandling{ false };
        bool m_suppressZoomHandling{ false };
        bool m_clearConfirmationOpen{ false };
        winrt::hstring m_monitoredEndpointName{};

        bool m_advancedMode{ false };
        bool m_advancedListening{ false };
        bool m_suppressModeToggle{ false };
        bool m_suppressSourceEvents{ false };
        bool m_usingSavedSources{ true };
        bool m_narrowLayout{ false };
        bool m_draggingSourcesSplitter{ false };
        double m_sourcesPaneWidth{ 284.0 };
        uint32_t m_sourcesSplitterPointerId{ 0 };
        double m_sourcesSplitterStartX{ 0 };
        double m_sourcesSplitterStartWidth{ 0 };
        std::vector<SourceRow> m_sourceRows{};
        std::map<std::wstring, SourceChoice> m_sourceChoices{};
        ::midi2monitor::AdvancedConnections m_advancedConnections{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_activityTimer{ nullptr };
        ::midiapp::MessageFilter m_messageFilter{};
        controls::Flyout m_filterFlyout{ nullptr };
    };
}

namespace winrt::midi2monitor::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
