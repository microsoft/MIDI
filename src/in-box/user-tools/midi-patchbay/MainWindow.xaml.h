// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "EndpointCatalog.h"
#include "PatchCanvas.h"
#include "PatchGraph.h"
#include "PatchModel.h"
#include "PatchStore.h"
#include "RouteEngine.h"
#include "ThemeBrushes.h"
#include "TrayIcon.h"

namespace winrt::midipatchbay::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void RestoreWindowPlacement() noexcept;
        void MinimizeAtStartup() noexcept;

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnAppearanceButtonClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAlwaysOnTopToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnNavigationSelectionChanged(
            controls::NavigationView const& sender,
            controls::NavigationViewSelectionChangedEventArgs const& args);

        void OnOpenFolderClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnNewPatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnNewQuickPatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSortClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSavePatchClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnPatchMenuClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRoutingToggleClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnAddEndpointClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCreateLoopbackClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnAutoArrangeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnFitClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomInClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomOutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomApplyClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnZoomFlyoutOpening(foundation::IInspectable const& sender, foundation::IInspectable const& args);
        void OnZoomValueChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnTestClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnShowLoopClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnInspectorCloseClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnSavePatchNameChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);
        void OnLoopbackNameChanged(foundation::IInspectable const& sender, controls::TextChangedEventArgs const& args);

        void OnQuickPatchSourceChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnQuickPatchDestinationChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnQuickPatchGroupChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);

    private:
        // ---- startup and chrome ----
        void InitializeWindowChrome() noexcept;
        void InitializeStaticText() noexcept;
        xaml::UIElement BuildAppSettingsPanel() noexcept;

        // ---- patch list and navigation ----
        void LoadPatches() noexcept;
        void RebuildNavigation() noexcept;
        void SelectPatch(_In_ std::wstring const& patchId) noexcept;

        // An empty preferred name gives the numbered untitled name.
        void CreateNewPatch(_In_ std::wstring const& preferredName = {}) noexcept;

        ::midipatchbay::PatchDocument* CurrentPatch() noexcept;

        // A patch's identity in the UI is its file path, or a session id while it has never been
        // written. Both are stable for as long as the patch exists.
        static std::wstring PatchKey(_In_ ::midipatchbay::PatchDocument const& patch) noexcept;

        // ---- canvas ----
        void RebuildCanvas() noexcept;
        void OnCanvasSelectionChanged() noexcept;
        void OnConnectionRequested(_In_ ::midipatchbay::PatchConnection connection) noexcept;
        void OnCanvasLayoutChanged() noexcept;
        void ShowEndpointMenu(_In_ std::wstring const& endpointId, _In_ foundation::Point const& position) noexcept;

        void ApplyZoom(_In_ float zoom) noexcept;
        void UpdateZoomText(_In_ float zoom) noexcept;

        void DeleteSelection() noexcept;
        winrt::fire_and_forget DeleteSelectionAsync();

        // ---- filters, in MainWindowFilters.cpp ----
        winrt::fire_and_forget ShowFilterDialogAsync(std::wstring connectionId);
        void BuildFilterDialog() noexcept;
        xaml::UIElement BuildNoteRangeSection() noexcept;
        void SetNoteRangeEnd(_In_ uint8_t note, _In_ bool isLow) noexcept;
        void RefreshNoteRangeUi() noexcept;
        void UpdateFilterSummary() noexcept;

        // ---- transforms, in MainWindowTransforms.cpp ----
        winrt::fire_and_forget ShowTransformDialogAsync(std::wstring connectionId);
        void BuildTransformDialog() noexcept;
        void RebuildNoteMapRows() noexcept;
        void RefreshNoteMapRowLabels() noexcept;
        void RebuildControlMapRows() noexcept;
        void CommitTransformMaps() noexcept;
        void UpdateTransformSummary() noexcept;
        void DrawVelocityCurve() noexcept;
        winrt::fire_and_forget PlayTestNoteAsync(_In_ uint8_t note);

        // ---- inspector, in MainWindowInspector.cpp ----
        void RefreshInspector() noexcept;
        void BuildEndpointInspector(_In_ ::midipatchbay::PatchEndpoint const& endpoint) noexcept;
        void BuildConnectionInspector(_In_ ::midipatchbay::PatchConnection const& connection) noexcept;

        // The activity line is the only part of the inspector that changes on its own, so it is
        // updated in place rather than rebuilding the panel on every timer tick.
        void UpdateInspectorActivity() noexcept;
        winrt::hstring ConnectionActivityText(_In_ std::wstring const& connectionId) const noexcept;

        // ---- dialogs and menus, in MainWindowDialogs.cpp ----
        winrt::fire_and_forget ShowSavePatchDialogAsync();
        winrt::fire_and_forget ShowCreateLoopbackDialogAsync();
        winrt::fire_and_forget ShowQuickPatchDialogAsync();
        winrt::fire_and_forget ShowDeletePatchDialogAsync();

        // The group lists depend on the endpoint picked above them, so they are filled after it.
        void FillQuickPatchGroups(_In_ bool isSource) noexcept;
        void ValidateQuickPatch() noexcept;
        void CreateQuickPatch() noexcept;
        void ShowEndpointPalette(_In_ xaml::FrameworkElement const& anchor) noexcept;
        void ShowTestMenu(_In_ xaml::FrameworkElement const& anchor) noexcept;
        void AddEndpointToPatch(_In_ ::midipatchbay::LiveEndpoint const& endpoint) noexcept;
        void AddRememberedEndpointToPatch(_In_ ::midipatchbay::PatchEndpoint const& remembered) noexcept;

        // Endpoints the customer has used in a saved patch but that are not here now. Derived
        // from the patches themselves rather than a separate list, so nothing extra is stored.
        std::vector<::midipatchbay::PatchEndpoint> RememberedEndpoints() const noexcept;

        void LaunchTool(_In_ std::wstring const& toolExeName, _In_ std::wstring const& endpointDeviceId) noexcept;

        // ---- routing, in MainWindowRouting.cpp ----
        void MarkDirty() noexcept;
        void AutoSaveIfDue() noexcept;
        void RefreshAnalysis() noexcept;
        void ApplyRouting() noexcept;
        void OnRefreshTimerTick() noexcept;
        void UpdateStatusStrip() noexcept;
        void UpdateMessages() noexcept;
        void UpdateTray() noexcept;
        void SetPatchRouting(_In_ std::wstring const& patchKey, _In_ bool routing) noexcept;
        void StopAllRouting() noexcept;

        void ShowStatus(_In_ winrt::hstring const& message, _In_ controls::InfoBarSeverity severity) noexcept;

        // Name, description, saved state and whether the patch is routing.
        void UpdatePatchHeader() noexcept;

        // ---- state ----
        midiapp::WindowChrome m_chrome{};
        ::midipatchbay::PatchCanvas m_canvas{};
        ::midipatchbay::TrayIcon m_tray{};

        std::vector<::midipatchbay::PatchDocument> m_patches{};
        std::wstring m_currentPatchKey{};

        // Patches whose routes are live. A patch can be open without routing, and routing
        // without being open, which is why this is a set rather than a flag on the document.
        std::unordered_set<std::wstring> m_routingPatchKeys{};

        ::midipatchbay::PatchAnalysis m_analysis{};
        std::vector<::midipatchbay::LiveEndpoint> m_liveEndpoints{};
        std::unordered_map<std::wstring, ::midipatchbay::RouteStats> m_routeStats{};

        controls::TextBlock m_activityText{ nullptr };
        std::wstring m_activityConnectionId{};

        // What the quick patch combos are showing, by position, because a ComboBox of strings
        // cannot carry an endpoint id or a group number on its own.
        std::vector<std::wstring> m_quickSourceIds{};
        std::vector<std::wstring> m_quickDestinationIds{};
        std::vector<int32_t> m_quickSourceGroups{};
        std::vector<int32_t> m_quickDestinationGroups{};
        bool m_fillingQuickPatch{ false };
        bool m_settingZoomBox{ false };

        // The filter dialog edits a copy, so Cancel leaves a live route untouched.
        ::midipatchbay::MessageFilter m_editingFilter{};
        std::wstring m_editingFilterConnectionId{};
        bool m_updatingNoteRange{ false };

        controls::NumberBox m_noteLowBox{ nullptr };
        controls::NumberBox m_noteHighBox{ nullptr };
        controls::CheckBox m_noteLimitCheck{ nullptr };
        controls::TextBlock m_noteRangeText{ nullptr };
        controls::Canvas m_noteKeyboard{ nullptr };
        std::vector<std::pair<uint8_t, shapes::Rectangle>> m_noteKeyFills{};

        // The transform dialog also edits a copy. The two tables are edited as row lists and
        // collected back into the sparse arrays on Apply.
        ::midipatchbay::MessageTransform m_editingTransform{};
        std::wstring m_editingTransformConnectionId{};

        std::vector<std::pair<int32_t, int32_t>> m_noteMapRows{};
        std::vector<std::pair<int32_t, int32_t>> m_controlMapRows{};

        controls::StackPanel m_noteMapPanel{ nullptr };
        controls::StackPanel m_controlMapPanel{ nullptr };
        std::vector<controls::TextBlock> m_noteMapLabels{};
        std::vector<controls::NumberBox> m_velocityRangeBoxes{};
        controls::TextBlock m_transposeExampleText{ nullptr };
        controls::Canvas m_velocityCurveCanvas{ nullptr };

        // Where the audition button plays, captured when the dialog opens.
        std::wstring m_testEndpointDeviceId{};
        int32_t m_testGroupIndex{ ::midipatchbay::AllGroups };

        uint64_t m_lastTotalMessages{ 0 };
        std::chrono::steady_clock::time_point m_lastRateSample{};

        // A patch that already has a file is written back a moment after it changes, so closing
        // the window never loses work the customer asked to keep. Temporary patches are not.
        std::unordered_set<std::wstring> m_unsavedPatchKeys{};
        std::chrono::steady_clock::time_point m_lastChangeTime{};

        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_rebuildingNavigation{ false };

        xaml::DispatcherTimer m_refreshTimer{ nullptr };

        winrt::event_token m_closedToken{};
    };
}

namespace winrt::midipatchbay::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
