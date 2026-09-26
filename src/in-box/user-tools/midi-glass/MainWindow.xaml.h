// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "WindowChrome.h"
#include "LibraryItems.h"
#include "LayoutModel.h"

namespace winrt::midiglass::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow() = default;

        // Called before Activate, so the window is sized and placed before its first paint.
        void RestoreWindowPlacement();

        void OnRootLoaded(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnAppearanceButtonClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnAlwaysOnTopToggled(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        // ---- the toolbar ----

        void OnNewLayoutClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnOpenFileClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnSearchTextChanged(
            controls::AutoSuggestBox const& sender,
            controls::AutoSuggestBoxTextChangedEventArgs const& args);

        void OnSortSelectionChanged(
            foundation::IInspectable const& sender,
            controls::SelectionChangedEventArgs const& args);

        void OnGridViewToggled(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnListViewToggled(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnLibrarySizeChanged(
            foundation::IInspectable const& sender,
            xaml::SizeChangedEventArgs const& args);

        // ---- the cards ----

        void OnLayoutItemClick(
            foundation::IInspectable const& sender,
            controls::ItemClickEventArgs const& args);

        void OnRunLayoutClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnEditLayoutClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnCardMoreClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnCardPointerEntered(
            foundation::IInspectable const& sender,
            xaml::Input::PointerRoutedEventArgs const& args);

        void OnCardPointerExited(
            foundation::IInspectable const& sender,
            xaml::Input::PointerRoutedEventArgs const& args);

        void OnCardGotFocus(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnCardLostFocus(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnGridGotFocus(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnGridLostFocus(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        // ---- the card menu ----

        void OnCardMenuOpening(
            foundation::IInspectable const& sender,
            foundation::IInspectable const& args);

        void OnCardMenuRun(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCardMenuEdit(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCardMenuDuplicate(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCardMenuRename(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCardMenuDescribe(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCardMenuFavorite(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCardMenuShowInFolder(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCardMenuDelete(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // ---- backing up, restoring and moving a layout (MainWindowPackaging.cpp) ----

        void OnCardMenuBackUp(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCardMenuRestore(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnCardMenuPackage(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnImportPackageClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnShowBackupsFolderClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnKeepAwakeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

    private:
        void OnWindowClosed(
            foundation::IInspectable const& sender,
            xaml::WindowEventArgs const& args);

        void RefreshLibrary();
        void ApplyCards(_In_ std::vector<::midiglass::LayoutCardData> const& cards);
        void RebuildSections();
        void ApplyViewMode();
        void ApplyItemWidths();
        void UpdateStatusBar();

        // The service can stop while the library is open, and a device watcher says nothing
        // about that on its own. This both polls and is called whenever a device arrives or
        // leaves, because a stopping service takes every device with it. Returns true when the
        // state changed, so the caller can decide whether the cards need re-reading.
        bool CheckServiceState();

        void RunCard(_In_ midiglass::LayoutCard const& card);
        void EditCard(_In_ midiglass::LayoutCard const& card);

        // Which backup to put back. A dialog rather than a straight overwrite, because there is
        // usually more than one and they are only told apart by their number.
        winrt::fire_and_forget RestoreCardAsync(_In_ midiglass::LayoutCard card);

        // Says plainly what happened. A backup nobody was told about is a backup nobody trusts.
        winrt::fire_and_forget ShowNoticeAsync(
            _In_ std::wstring title,
            _In_ std::wstring body);

        // Reads, changes and writes one layout, then refreshes. Everything the card menu does to
        // a file goes through here, so there is one place that knows a save can fail.
        bool EditLayoutFile(
            _In_ std::wstring const& filePath,
            _In_ std::function<void(glass::LayoutDocument&)> const& change);

        foundation::IAsyncAction ShowNewLayoutDialogAsync();
        foundation::IAsyncAction RenameCardAsync(_In_ midiglass::LayoutCard card);
        foundation::IAsyncAction DescribeCardAsync(_In_ midiglass::LayoutCard card);
        foundation::IAsyncAction DeleteCardAsync(_In_ midiglass::LayoutCard card);

        midiapp::WindowChrome m_chrome{};

        // Everything read from disk, before the search and the sort are applied.
        std::vector<::midiglass::LayoutCardData> m_allCards{};

        // What the last read produced. The endpoint watcher fires once per endpoint on the
        // machine at startup and again whenever anything is plugged in, so a rebuild only
        // happens when something a card actually shows has changed.
        std::wstring m_cardSignature{};

        collections::IObservableVector<foundation::IInspectable> m_favorites{
            winrt::single_threaded_observable_vector<foundation::IInspectable>() };

        collections::IObservableVector<foundation::IInspectable> m_recent{
            winrt::single_threaded_observable_vector<foundation::IInspectable>() };

        // Which card the context menu was opened on. One menu is shared by every card, so this
        // is what tells the handlers which layout they are acting on.
        midiglass::LayoutCard m_menuCard{ nullptr };

        std::wstring m_searchText{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

        xaml::DispatcherTimer m_serviceTimer{ nullptr };
        bool m_serviceRunning{ false };

        bool m_refreshing{ false };
        bool m_updatingChrome{ false };
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
