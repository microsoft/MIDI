// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "SynthHost.h"
#include "SynthItem.h"
#include "SynthStore.h"
#include "TrayIcon.h"
#include "WindowChrome.h"

namespace winrt::midisoundmodule::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void RestoreWindowPlacement() noexcept;
        void MinimizeAtStartup() noexcept;

        void OnRootLoaded(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSettingsToggleClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnAlwaysOnTopToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnAddSynthClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSilenceClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnMoreClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnSynthToggled(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnVolumeChanged(_In_ foundation::IInspectable const& sender, _In_ controls::Primitives::RangeBaseValueChangedEventArgs const& args);
        void OnRenameTextChanged(_In_ foundation::IInspectable const& sender, _In_ controls::TextChangedEventArgs const& args);
        void OnRootDragOver(_In_ foundation::IInspectable const& sender, _In_ xaml::DragEventArgs const& args);
        winrt::fire_and_forget OnRootDrop(_In_ foundation::IInspectable sender, _In_ xaml::DragEventArgs args);

    private:
        void InitializeWindowChrome() noexcept;
        void InitializeNotificationArea() noexcept;
        void HookWindowEvents() noexcept;

        void ApplyAudioSettings() noexcept;
        xaml::UIElement BuildSettingsPanel() noexcept;

        ::midisoundmodule::SynthDefinition* FindDefinition(_In_ std::wstring const& id) noexcept;
        winrt::midisoundmodule::SynthItem FindItem(_In_ std::wstring const& id) noexcept;
        void AddItem(_In_ ::midisoundmodule::SynthDefinition const& definition);
        void ApplyDefinitionToItem(_In_ ::midisoundmodule::SynthDefinition const& definition) noexcept;

        void QueueRefresh() noexcept;
        void RefreshFromHost() noexcept;
        void UpdateEmptyState() noexcept;
        void RequestSave() noexcept;
        void SaveNow() noexcept;

        void SetSynthEnabled(_In_ std::wstring const& id, _In_ bool enabled) noexcept;

        winrt::fire_and_forget AddSynthAsync();
        bool AddSynthFromFile(_In_ std::wstring const& path) noexcept;
        winrt::fire_and_forget RenameSynthAsync(_In_ std::wstring id);
        winrt::fire_and_forget RemoveSynthAsync(_In_ std::wstring id);
        void ShowInFolder(_In_ std::wstring const& id) noexcept;

        void ShowNotice(_In_ winrt::hstring const& message, _In_ controls::InfoBarSeverity severity) noexcept;

        winrt::hstring StatusText(_In_ ::midisoundmodule::SynthStatus const& status) const;
        winrt::hstring AudioStatusText(_In_ SoundFontSynth::AudioEngineStatus const& status, _In_ bool anyRunning) const;

        bool TryHideToNotificationArea() noexcept;
        void RestoreFromNotificationArea() noexcept;
        void UpdateTray() noexcept;
        winrt::fire_and_forget ConfirmExitAsync();
        void ExitApp() noexcept;

        HWND WindowHandle() const noexcept;

        midiapp::WindowChrome m_chrome{};
        ::midisoundmodule::TrayIcon m_tray{};

        std::vector<::midisoundmodule::SynthDefinition> m_definitions{};
        collections::IObservableVector<winrt::midisoundmodule::SynthItem> m_items{ nullptr };
        std::map<std::wstring, ::midisoundmodule::SynthStatus> m_statuses{};

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };
        xaml::DispatcherTimer m_refreshTimer{ nullptr };
        std::shared_ptr<std::atomic<bool>> m_refreshQueued{ std::make_shared<std::atomic<bool>>(false) };

        bool m_loaded{ false };
        bool m_exiting{ false };
        bool m_closing{ false };
        bool m_restoringFromNotificationArea{ false };
        bool m_dialogOpen{ false };

        bool m_saveRequested{ false };
        uint64_t m_saveDueTick{ 0 };

        winrt::event_token m_closingToken{};
        winrt::event_token m_windowChangedToken{};
    };
}

namespace winrt::midisoundmodule::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
