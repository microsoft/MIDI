// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LibraryWindow.g.h"

#include "AppSettings.h"
#include "PatchLibrary.h"
#include "ThemeBrushes.h"
#include "TrayIcon.h"

namespace winrt::midipatchbay::implementation
{
    // The app's main window: every patch as a tile, and what belongs to the app rather than to
    // one patch, such as the notification area, the settings, importing and the assistant. Each
    // patch opens in an editor window of its own.
    struct LibraryWindow : LibraryWindowT<LibraryWindow>
    {
        LibraryWindow();

        void RestoreWindowPlacement() noexcept;
        void MinimizeAtStartup() noexcept;

        // Back from the notification area, or from behind other windows.
        void BringForward() noexcept;

        void OnRootLoaded(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnAppearanceButtonClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnAlwaysOnTopToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnNewPatchClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnNewQuickPatchClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportPatchClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnAssistantClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnOpenFolderClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSortSelectionChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnGridViewToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnListViewToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        void OnSearchTextChanged(
            _In_ controls::AutoSuggestBox const& sender,
            _In_ controls::AutoSuggestBoxTextChangedEventArgs const& args);
        void OnFilterClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnPatchTileClick(_In_ foundation::IInspectable const& sender, _In_ controls::ItemClickEventArgs const& args);
        void OnPatchGridSizeChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::SizeChangedEventArgs const& args);

        void OnQuickPatchSourceChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnQuickPatchDestinationChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnQuickPatchGroupChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);

    private:
        // ---- startup and chrome, in LibraryWindow.xaml.cpp ----
        void InitializeWindowChrome() noexcept;
        void ApplyAssistantVisibility() noexcept;
        xaml::UIElement BuildAppSettingsPanel() noexcept;

        // ---- notification area ----
        void InitializeNotificationArea() noexcept;

        // True when the close was turned into a hide, which is what keeps the routes running.
        // False means the window really is going away.
        bool TryHideToNotificationArea() noexcept;

        void RestoreFromNotificationArea() noexcept;
        void UpdateTray() noexcept;

        // Closing the library closes the app when there is no notification area icon to keep it
        // running, and that stops every route, so it asks first when anything is routing.
        winrt::fire_and_forget ConfirmExitAsync();
        void ExitApp() noexcept;

        // ---- documents from Explorer ----
        // A patch double-clicked while the app is open arrives from the second copy as
        // WM_COPYDATA, which XAML does not pass on, so the window is subclassed to see it.
        static LRESULT CALLBACK HandoffSubclassProcedure(
            _In_ HWND window,
            _In_ UINT message,
            _In_ WPARAM wParam,
            _In_ LPARAM lParam,
            _In_ UINT_PTR subclassId,
            _In_ DWORD_PTR referenceData) noexcept;

        static winrt::weak_ref<LibraryWindow> s_instance;

        // Each file is copied into the patch folder and none of them routes. The last one opens.
        // False when none of them could be imported.
        bool ImportPatchFiles(_In_ std::vector<std::wstring> const& paths) noexcept;

        // ---- tiles, rows, the filter and the bar at the bottom, in LibraryWindowTiles.cpp ----
        enum class LibraryFilter
        {
            All,
            Routing,
            Stopped,
            Attention,
        };

        void InitializeLibraryControls() noexcept;
        void ApplyViewMode() noexcept;

        void OnLibraryChanged(_In_ ::midipatchbay::LibraryChange change, _In_ std::wstring const& key) noexcept;
        void RebuildTiles() noexcept;
        xaml::UIElement BuildTile(_In_ ::midipatchbay::PatchDocument const& patch) noexcept;
        xaml::UIElement BuildListRow(_In_ ::midipatchbay::PatchDocument const& patch) noexcept;
        xaml::UIElement BuildNewTile() noexcept;
        xaml::UIElement BuildMiniMap(_In_ ::midipatchbay::PatchDocument const& patch, _In_ double width, _In_ double height) noexcept;
        controls::MenuFlyout BuildTileMenu(_In_ std::wstring const& key) noexcept;
        void ShowNewPatchMenu(_In_ xaml::FrameworkElement const& anchor) noexcept;

        // The routing switches, edges and rates change without the tiles being built again.
        void RefreshTileStates() noexcept;
        void UpdateTileRates() noexcept;
        void UpdateFilterCounts() noexcept;
        void ShowHoverBar(_In_ std::wstring const& key, _In_ bool show) noexcept;

        // A row is as wide as the list, which a row cannot work out for itself.
        void ApplyRowWidths() noexcept;

        // Missing endpoints, a loop that is held muted, or routing the app had to refuse.
        bool NeedsAttention(_In_ ::midipatchbay::PatchDocument const& patch) noexcept;

        void OpenPatch(_In_ std::wstring const& key) noexcept;
        void DuplicatePatch(_In_ std::wstring const& key) noexcept;
        winrt::fire_and_forget DeletePatchAsync(std::wstring key);
        void ShowInFolder(_In_ std::wstring const& path) noexcept;

        void OnRefreshTimerTick() noexcept;
        void UpdateStatusStrip() noexcept;
        void CheckServiceState() noexcept;
        void UpdateServiceChip() noexcept;
        void ShowStatus(_In_ winrt::hstring const& message, _In_ controls::InfoBarSeverity severity) noexcept;

        // ---- quick patch and the assistant, in LibraryWindowDialogs.cpp ----
        winrt::fire_and_forget ShowQuickPatchDialogAsync();

        // The group lists depend on the endpoint picked above them, so they are filled after it.
        void FillQuickPatchGroups(_In_ bool isSource) noexcept;
        void ValidateQuickPatch() noexcept;
        void CreateQuickPatch() noexcept;

        winrt::fire_and_forget ShowAssistantDialogAsync();

        // ---- state ----
        midiapp::WindowChrome m_chrome{};
        ::midipatchbay::TrayIcon m_tray{};

        uint32_t m_libraryToken{ 0 };
        xaml::DispatcherTimer m_refreshTimer{ nullptr };

        // What each tile or row needs to change in place, by patch.
        struct TileParts
        {
            std::wstring Key{};
            controls::ToggleSwitch Routing{ nullptr };
            shapes::Rectangle Outline{ nullptr };
            xaml::UIElement RatePill{ nullptr };
            controls::TextBlock RateText{ nullptr };
            xaml::UIElement HoverBar{ nullptr };
            xaml::FrameworkElement Root{ nullptr };
        };

        std::vector<TileParts> m_tiles{};
        bool m_updatingTiles{ false };
        LibraryFilter m_filter{ LibraryFilter::All };
        bool m_fillingSortSelector{ false };

        // The last count each tile's rate was worked out from.
        struct RateSample
        {
            uint64_t Delivered{ 0 };
            std::chrono::steady_clock::time_point At{};
        };

        std::unordered_map<std::wstring, RateSample> m_rateSamples{};

        bool m_serviceRunning{ true };
        uint32_t m_serviceCheckTicks{ 0 };

        // What the quick patch combos are showing, by position, because a ComboBox of strings
        // cannot carry an endpoint id or a group number on its own.
        std::vector<std::wstring> m_quickSourceIds{};
        std::vector<std::wstring> m_quickDestinationIds{};
        std::vector<int32_t> m_quickSourceGroups{};
        std::vector<int32_t> m_quickDestinationGroups{};
        bool m_fillingQuickPatch{ false };

        uint64_t m_lastDelivered{ 0 };
        std::chrono::steady_clock::time_point m_lastRateSample{};

        // Shown once, rather than every time the timer sees it.
        winrt::hstring m_lastEngineError{};

        bool m_loaded{ false };
        bool m_closing{ false };

        // Set once the customer has chosen to close the app, so the close is not stopped again.
        bool m_exiting{ false };

        // Guards the window briefly reporting itself minimized on its way out of the
        // notification area, which would otherwise hide it again straight away.
        bool m_restoringFromNotificationArea{ false };

        winrt::event_token m_closedToken{};
        winrt::event_token m_closingToken{};
        winrt::event_token m_windowChangedToken{};
    };
}

namespace winrt::midipatchbay::factory_implementation
{
    struct LibraryWindow : LibraryWindowT<LibraryWindow, implementation::LibraryWindow>
    {
    };
}
