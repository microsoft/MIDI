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

        // ---- the library ----

        void OnNewLayoutClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnOpenFolderClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnRefreshClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

        void OnLayoutItemClick(
            foundation::IInspectable const& sender,
            controls::ItemClickEventArgs const& args);

        void OnRunLayoutClick(
            foundation::IInspectable const& sender,
            xaml::RoutedEventArgs const& args);

    private:
        void OnWindowClosed(
            foundation::IInspectable const& sender,
            xaml::WindowEventArgs const& args);

        void RefreshLibrary();
        void ApplyCards(_In_ std::vector<::midiglass::LayoutCardData> const& cards);

        foundation::IAsyncAction ShowNewLayoutDialogAsync();

        midiapp::WindowChrome m_chrome{};

        collections::IObservableVector<foundation::IInspectable> m_cards{
            winrt::single_threaded_observable_vector<foundation::IInspectable>() };

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };

        bool m_refreshing{ false };
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
