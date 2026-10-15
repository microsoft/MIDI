// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This window reads and writes the Windows MIDI Services
// configuration file directly, through the configuration engine. Microsoft does not support direct
// manipulation of that file by anything other than the in-box MIDI tools. Its format, its name, its
// location and the registry value that selects it are implementation details and can change in any
// release without notice. Applications and third-party tools must use the Windows MIDI Services API
// (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

#pragma once

#include "ConfigurationsWindow.g.h"

#include "ConfigurationsSupport.h"
#include "ConfigPackage.h"
#include "ImportApply.h"
#include "ImportPlan.h"
#include "WindowChrome.h"

namespace winrt::midisettings::implementation
{
    // Everything people do with whole configurations: switch, back up, restore, share and import.
    // One of these at a time, owned by the main window and closed along with it. The pages are in
    // ConfigurationsWindow<Page>.cpp.
    struct ConfigurationsWindow : ConfigurationsWindowT<ConfigurationsWindow>
    {
        ConfigurationsWindow();

        // runs before Activate, so it can't touch the chrome instance
        void RestoreWindowPlacement() noexcept;

        // Brings the window forward, and back from the taskbar when it was minimized.
        void BringToFront() noexcept;

        // By the names the launch switch takes: configurations, backups, share or import. Before
        // the window has loaded, it's kept until it has.
        void ShowPage(_In_ std::wstring_view page) noexcept;

        // Opens a package on the Import page. Before the window has loaded, it's kept until it has.
        void OpenPackage(_In_ std::wstring const& path) noexcept;

        // The main window's appearance or always on top setting changed.
        void ApplyAppearance() noexcept;
        void ApplyAlwaysOnTop() noexcept;

        // Reopen as administrator asks the main window, which owns the app's single instance, to
        // start the elevated copy with these switches. True when it did, and this window closes.
        void SetRelaunchHandler(_In_ std::function<bool(std::wstring const&)> handler) noexcept;

        void OnRootLoaded(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnRootSizeChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::SizeChangedEventArgs const& args);
        void OnNavigationSelectionChanged(_In_ controls::NavigationView const& sender, _In_ controls::NavigationViewSelectionChangedEventArgs const& args);
        void OnReopenElevatedClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRestartServiceClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        // Configurations
        winrt::fire_and_forget OnRenameInUseClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnKeepUsingClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnBackUpInUseClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnShareInUseClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnNewEmptyClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnNewCopyClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        // Backups
        winrt::fire_and_forget OnBackUpNowClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDeleteOldBackupsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnBackupFilterChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnBackupSelectionChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        void OnShowOlderBackupsClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnRestoreBackupClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnShareBackupClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnDeleteBackupClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        // Share
        void OnShareConfigurationChanged(_In_ foundation::IInspectable const& sender, _In_ controls::SelectionChangedEventArgs const& args);
        winrt::fire_and_forget OnSavePackageClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnShowInFolderClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

        // Import
        void OnImportDragOver(_In_ foundation::IInspectable const& sender, _In_ xaml::DragEventArgs const& args);
        winrt::fire_and_forget OnImportDrop(_In_ foundation::IInspectable const& sender, _In_ xaml::DragEventArgs const& args);
        void OnChoosePackageClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportTargetChanged(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportCancelClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnImportClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        void OnImportDoneClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget OnUndoImportClick(_In_ foundation::IInspectable const& sender, _In_ xaml::RoutedEventArgs const& args);

    private:
        // --- the window (ConfigurationsWindow.xaml.cpp) ---
        HWND WindowHandle() noexcept;
        void SaveWindowPlacement() noexcept;
        void SelectPage(_In_ std::wstring_view page) noexcept;
        std::wstring CurrentPageName() noexcept;

        // Reads the folder, the backups and the service again, then redraws every page.
        winrt::fire_and_forget Refresh() noexcept;
        foundation::IAsyncAction RefreshAsync();
        void RenderAll() noexcept;

        // Only one change at a time, and none while the window is closing.
        bool BeginWork() noexcept;
        void EndWork() noexcept;
        void ApplyWorkState() noexcept;

        // Messages at the top of a page.
        void ShowStatus(
            _In_ controls::InfoBar const& bar,
            _In_ controls::InfoBarSeverity severity,
            _In_ winrt::hstring const& title,
            _In_ winrt::hstring const& message) noexcept;
        void ShowFailure(
            _In_ controls::InfoBar const& bar,
            _In_ winrt::hstring const& title,
            _In_ ::midisettings::configengine::ConfigResult const& result) noexcept;
        void ShowSaved(_In_ controls::InfoBar const& bar, _In_ std::wstring const& path) noexcept;

        // Dialogs, made in code because what they say depends on what's picked.
        controls::ContentDialog MakeDialog(
            _In_ winrt::hstring const& title,
            _In_ winrt::hstring const& primary,
            _In_ winrt::hstring const& secondary,
            _In_ winrt::hstring const& close) noexcept;
        controls::TextBlock MakeDialogText(_In_ winrt::hstring const& text, _In_ bool caption) noexcept;

        // Pieces the pages build in code, styled from the window's resources so their colors
        // follow the theme. kind is empty, Accent or Caution.
        xaml::Style StyleNamed(_In_ std::wstring_view key) noexcept;
        controls::TextBlock MakeText(_In_ winrt::hstring const& text, _In_ std::wstring_view styleKey);
        controls::FontIcon MakeGlyph(_In_ wchar_t glyph, _In_ std::wstring_view styleKey);
        controls::Grid MakeChip(_In_ winrt::hstring const& text, _In_ std::wstring_view kind);

        // A name for a new, renamed or duplicated configuration. Empty when canceled.
        foundation::IAsyncOperation<winrt::hstring> AskForNameAsync(
            winrt::hstring title,
            winrt::hstring primary,
            winrt::hstring initialName,
            std::wstring renamingFileName);

        // Restarts the MIDI service and forgets what was waiting for it.
        foundation::IAsyncAction RestartServiceAsync(controls::InfoBar statusBar);

        void RecordPendingRestart(_In_ uint32_t kinds, _In_ std::wstring const& runningFileName) noexcept;

        ::midisettings::configengine::ConfigFileEntry const* FindConfiguration(_In_ std::wstring const& fileName) const noexcept;

        // The configuration the MIDI service is using now: the one picked, or the one it still runs
        // until a switch takes effect.
        std::wstring RunningFileName() const noexcept;

        // --- Configurations (ConfigurationsWindowConfigurations.cpp) ---
        void RenderConfigurationsPage() noexcept;
        void RenderInUseCard(_In_ ::midisettings::configengine::ConfigFileEntry const& entry) noexcept;
        xaml::UIElement MakeConfigurationRow(_In_ ::midisettings::configengine::ConfigFileEntry const& entry);
        winrt::hstring ChangedText(_In_ ::midisettings::configengine::ConfigFileEntry const& entry, _In_ bool withTime) const;
        void FillContentsText(_In_ controls::RichTextBlock const& block, _In_ ::midisettings::configengine::ConfigInventory const& inventory);
        winrt::fire_and_forget SwitchToAsync(std::wstring fileName);
        winrt::fire_and_forget RenameAsync(std::wstring fileName);
        winrt::fire_and_forget CreateAsync(std::wstring copyOfFileName);
        winrt::fire_and_forget BackUpAsync(std::wstring fileName, controls::InfoBar statusBar);
        winrt::fire_and_forget DeleteConfigurationAsync(std::wstring fileName);
        void ShareConfiguration(_In_ std::wstring const& fileName) noexcept;

        // --- Backups (ConfigurationsWindowBackups.cpp) ---
        void RenderBackupsPage() noexcept;
        void RenderBackupList() noexcept;
        void RenderBackupDetail() noexcept;
        winrt::fire_and_forget LoadBackupDetailAsync(::midisettings::configengine::BackupEntry backup);
        controls::ListViewItem MakeBackupRow(_In_ ::midisettings::configengine::BackupEntry const& backup);
        winrt::hstring BackupReasonText(_In_ ::midisettings::configengine::BackupEntry const& backup, _In_ bool sentence) const;

        // --- Share (ConfigurationsWindowShare.cpp) ---
        void RenderSharePage() noexcept;
        void RenderShareIncludes() noexcept;

        // --- Import (ConfigurationsWindowImport.cpp, ConfigurationsWindowImportReview.cpp) ---
        enum class ImportPanel
        {
            Start,
            Review,
            Done,
        };

        foundation::IAsyncAction OpenPackageAsync(std::wstring path);
        winrt::fire_and_forget ChoosePackage() noexcept;
        void ShowImportPanel(_In_ ImportPanel panel) noexcept;
        void RenderImportReview() noexcept;
        void RenderImportTarget() noexcept;
        void RenderImportParts() noexcept;
        void RenderImportFooter() noexcept;
        void RenderImportDone() noexcept;
        void ReRenderImportPartsLater(_In_ std::wstring focusKey) noexcept;
        void ResetImport() noexcept;

        xaml::UIElement MakeImportPart(
            _In_ ::midisettings::configengine::ConfigPart part,
            _In_ std::vector<::midisettings::configengine::PlanItem const*> const& items);
        void AddDeviceNameItems(
            _In_ controls::StackPanel const& body,
            _In_ std::vector<::midisettings::configengine::PlanItem const*> const& items);
        void AddEntryItems(
            _In_ controls::StackPanel const& body,
            _In_ std::vector<::midisettings::configengine::PlanItem const*> const& items);
        void AddOtherSettingsItems(
            _In_ controls::StackPanel const& body,
            _In_ std::vector<::midisettings::configengine::PlanItem const*> const& items);
        xaml::FrameworkElement MakeImportItem(
            _In_ wchar_t glyph,
            _In_ std::wstring_view toneKey,
            _In_ xaml::UIElement const& picture,
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& what,
            _In_ xaml::UIElement const& right);
        xaml::FrameworkElement MakeImportGroupRow(_In_ winrt::hstring const& text, _In_ xaml::UIElement const& right);
        xaml::FrameworkElement MakeImportNote(_In_ winrt::hstring const& text);
        xaml::UIElement MakeShowAllLink(_In_ std::wstring const& groupKey, _In_ size_t count);
        controls::ComboBox MakePartCombo(
            _In_ std::wstring const& focusKey,
            _In_ std::vector<winrt::hstring> const& options,
            _In_ int32_t selected,
            _In_ winrt::hstring const& accessibleName,
            _In_ std::function<void(int32_t)> onChanged);
        controls::CheckBox MakeItemCheck(
            _In_ std::wstring const& focusKey,
            _In_ winrt::hstring const& label,
            _In_ bool checked,
            _In_ std::function<void(bool)> onChanged);
        void RememberFocusTarget(_In_ std::wstring const& key, _In_ controls::Control const& control);
        xaml::UIElement MakeImportPicture(_In_ std::wstring const& packagePicture, _In_ std::wstring const& herePicture);
        bool IsPluggedIn(_In_ ::midisettings::configengine::PlanItem const& item) const noexcept;
        winrt::hstring DeviceDisplayName(_In_ ::midisettings::configengine::PlanItem const& item) const;
        winrt::hstring ResultLineText(_In_ ::midisettings::configengine::ResultLine const& line) const;
        winrt::hstring ImportPackageName() const;

        midiapp::WindowChrome m_chrome{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        std::function<bool(std::wstring const&)> m_relaunchHandler{};

        bool m_loaded{ false };
        bool m_closing{ false };
        bool m_elevated{ false };
        bool m_busy{ false };
        bool m_refreshing{ false };
        bool m_refreshAgain{ false };
        bool m_suppressNavigation{ false };

        // asked for before the window loaded
        std::wstring m_pageOnLoad{};
        std::wstring m_packageOnLoad{};

        ::midisettings::configengine::ConfigEnvironment m_environment{};

        // the last read of the folder, the backups and the service
        std::vector<::midisettings::configengine::ConfigFileEntry> m_configurations{};
        std::vector<::midisettings::configengine::BackupEntry> m_backups{};
        std::wstring m_currentFileName{};
        uint32_t m_serviceProcessId{ 0 };
        ::midisettings::configurations::PendingRestart m_pending{};

        // Backups
        std::wstring m_backupFilter{};
        bool m_showOlderBackups{ false };
        bool m_suppressBackupHandling{ false };
        std::vector<std::optional<size_t>> m_backupListIndexes{};
        std::optional<::midisettings::configengine::BackupEntry> m_selectedBackup{};
        std::optional<::midisettings::configengine::ConfigInventory> m_selectedBackupInventory{};
        uint32_t m_selectedBackupPictures{ 0 };
        bool m_selectedBackupReadFailed{ false };
        uint64_t m_backupDetailGeneration{ 0 };

        // Share
        std::wstring m_shareFileName{};
        std::wstring m_sharePackageNameFor{};
        bool m_suppressShareHandling{ false };
        std::vector<std::pair<::midisettings::configengine::ConfigPart, controls::CheckBox>> m_shareChecks{};
        std::wstring m_shareChecksFor{};
        std::wstring m_savedPackagePath{};

        // Import
        ImportPanel m_importPanel{ ImportPanel::Start };
        std::wstring m_importPath{};
        ::midisettings::configengine::PackageContents m_importContents{};
        ::midisettings::configengine::ImportPlan m_importPlan{};
        ::midisettings::configengine::ImportChoices m_importChoices{};
        std::vector<::midisettings::configurations::LiveEndpoint> m_liveEndpoints{};
        std::vector<std::wstring> m_expandedImportParts{};
        std::vector<std::wstring> m_showAllImportGroups{};
        bool m_renderImportPending{ false };
        bool m_importHasWork{ false };
        std::wstring m_importFocusKey{};
        std::vector<std::pair<std::wstring, controls::Control>> m_importFocusTargets{};
        std::optional<::midisettings::configengine::ImportResult> m_importResult{};
        ::midisettings::configurations::PendingRestart m_pendingBeforeImport{};
        uint32_t m_importServiceProcessId{ 0 };
        ::midisettings::configengine::PortNaming m_portNamingBeforeImport{ ::midisettings::configengine::PortNaming::None };
        bool m_importUndone{ false };
    };
}

namespace winrt::midisettings::factory_implementation
{
    struct ConfigurationsWindow : ConfigurationsWindowT<ConfigurationsWindow, implementation::ConfigurationsWindow>
    {
    };
}
