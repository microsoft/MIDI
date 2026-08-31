// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "App.xaml.h"
#include "BackgroundWork.h"
#include "Elevation.h"
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;

namespace winrt::midisettings::implementation
{
    namespace
    {
        // The Win32 dialog rather than Windows.Storage.Pickers, for the same reason the picture
        // picker uses it: the WinRT picker's completion never resumes when it is raised over an
        // open ContentDialog, and this one always is.
        std::wstring ShowSaveDialog(
            _In_ HWND const owner,
            _In_ std::wstring const& suggestedFileName,
            _In_ std::wstring const& startFolder) noexcept
        {
            try
            {
                winrt::com_ptr<IFileSaveDialog> dialog{};

                if (FAILED(::CoCreateInstance(
                    CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
                {
                    return {};
                }

                COMDLG_FILTERSPEC const filters[]
                {
                    { L"MIDI configuration", L"*.midiconfig.json" },
                    { L"All files", L"*.*" },
                };

                LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
                LOG_IF_FAILED(dialog->SetOptions(FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM));

                if (!suggestedFileName.empty())
                {
                    LOG_IF_FAILED(dialog->SetFileName(suggestedFileName.c_str()));
                }

                if (!startFolder.empty())
                {
                    winrt::com_ptr<IShellItem> folder{};

                    if (SUCCEEDED(::SHCreateItemFromParsingName(
                        startFolder.c_str(), nullptr, IID_PPV_ARGS(folder.put()))))
                    {
                        LOG_IF_FAILED(dialog->SetDefaultFolder(folder.get()));
                    }
                }

                // canceling is reported as a failure hresult, so this is not logged as an error
                if (FAILED(dialog->Show(owner)))
                {
                    return {};
                }

                winrt::com_ptr<IShellItem> item{};

                if (FAILED(dialog->GetResult(item.put())))
                {
                    return {};
                }

                wil::unique_cotaskmem_string path{};

                if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
                {
                    return {};
                }

                return std::wstring{ path.get() };
            }
            catch (...)
            {
                return {};
            }
        }

        std::wstring ParentFolder(_In_ std::wstring const& path) noexcept
        {
            auto const separator = path.find_last_of(L"\\/");

            return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator);
        }
    }

    // The whole point of the banner is that a customer who has never opened this app has no
    // configuration file, so nothing they set here would survive a restart.
    void MainWindow::ShowFirstRunInvitation() noexcept
    {
        try
        {
            FirstRunBar().IsOpen(native::config::CurrentFullPath().empty());
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to check for a configuration file.")
    }

    void MainWindow::RefreshGlobalSettings() noexcept
    {
        try
        {
            auto const elevated = ::winrt::midisettings::implementation::App::IsElevated();

            GlobalElevationBar().IsOpen(!elevated);

            ConfigFileApplyButton().IsEnabled(elevated);
            CreateConfigButton().IsEnabled(elevated);
            NewConfigNameTextBox().IsEnabled(elevated);
            PortNamingClassicRadio().IsEnabled(elevated);
            PortNamingNewStyleRadio().IsEnabled(elevated);
            RestartServiceButton().IsEnabled(elevated);

            auto const files = native::config::EnumerateFiles();

            m_configFileChoices.Clear();

            int32_t selectedIndex{ -1 };
            int32_t index{ 0 };

            for (auto const& file : files)
            {
                m_configFileChoices.Append(winrt::make<implementation::ConfigFileChoice>(
                    file.IsCurrent ?
                        res::FormatString(L"ConfigFileCurrentFormat", winrt::hstring{ file.ConfigName }) :
                        winrt::hstring{ file.ConfigName },
                    winrt::hstring{ file.FileName }));

                if (file.IsCurrent)
                {
                    selectedIndex = index;
                }

                index++;
            }

            ConfigFileComboBox().SelectedIndex(selectedIndex);

            CopyConfigButton().IsEnabled(!native::config::CurrentFullPath().empty());

            m_suppressPortNamingHandling = true;

            auto const naming = native::config::DefaultMidi1PortNaming();

            PortNamingNewStyleRadio().IsChecked(naming == native::Midi1PortNaming::NewStyle);
            PortNamingClassicRadio().IsChecked(naming != native::Midi1PortNaming::NewStyle);

            m_suppressPortNamingHandling = false;

            GlobalStatusText().Text({});
        }
        catch (...)
        {
            m_suppressPortNamingHandling = false;

            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to load the global settings.");
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnGlobalSettingsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            RefreshGlobalSettings();

            GlobalSettingsDialog().XamlRoot(Content().XamlRoot());

            co_await GlobalSettingsDialog().ShowAsync();

            if (m_closing)
            {
                co_return;
            }

            ShowFirstRunInvitation();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the global settings.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnApplyConfigFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const selected = ConfigFileComboBox().SelectedItem()
                .try_as<midisettings::ConfigFileChoice>();

            if (selected == nullptr)
            {
                GlobalStatusText().Text(res::GetString(L"ConfigFileNoSelection"));
                co_return;
            }

            std::wstring errorMessage{};

            if (!native::config::SetCurrentFileName(std::wstring{ selected.FileName() }, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"ConfigFileFailedFormat", winrt::hstring{ errorMessage }));
                co_return;
            }

            GlobalStatusText().Text(res::GetString(L"ConfigFileChangedRestartNeeded"));

            RefreshGlobalSettings();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the configuration file.")

        co_return;
    }

    _Use_decl_annotations_
    void MainWindow::OnCreateConfigFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            std::wstring const configName{ NewConfigNameTextBox().Text() };

            auto const fileName = native::config::FileNameFromConfigName(configName);

            if (fileName.empty())
            {
                GlobalStatusText().Text(res::GetString(L"ConfigNameNotUsable"));
                return;
            }

            std::wstring errorMessage{};

            if (!native::config::CreateFile(configName, fileName, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"ConfigFileFailedFormat", winrt::hstring{ errorMessage }));
                return;
            }

            // A file nobody is using helps no one, so creating one also makes it the active one.
            if (!native::config::SetCurrentFileName(fileName, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"ConfigFileFailedFormat", winrt::hstring{ errorMessage }));
                return;
            }

            NewConfigNameTextBox().Text({});

            RefreshGlobalSettings();

            GlobalStatusText().Text(res::GetString(L"ConfigFileCreatedRestartNeeded"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to create the configuration file.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyConfigFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const source = native::config::CurrentFullPath();

            if (source.empty())
            {
                GlobalStatusText().Text(res::GetString(L"ConfigFileNoneActive"));
                return;
            }

            std::wstring suggested{ source };

            auto const separator = suggested.find_last_of(L"\\/");

            if (separator != std::wstring::npos)
            {
                suggested = suggested.substr(separator + 1);
            }

            auto const destination = ShowSaveDialog(
                WindowHandle(), suggested, native::AppSettings::Current().LastConfigCopyFolder());

            if (destination.empty())
            {
                return;
            }

            std::wstring errorMessage{};

            if (!native::config::CopyCurrentFileTo(destination, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"ConfigFileFailedFormat", winrt::hstring{ errorMessage }));
                return;
            }

            native::AppSettings::Current().LastConfigCopyFolder(ParentFolder(destination));

            GlobalStatusText().Text(res::FormatString(
                L"ConfigFileCopiedFormat", winrt::hstring{ destination }));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to copy the configuration file.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnPortNamingChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (m_suppressPortNamingHandling)
            {
                co_return;
            }

            auto const newStyle = PortNamingNewStyleRadio().IsChecked();

            auto const value = newStyle && newStyle.Value() ?
                native::Midi1PortNaming::NewStyle : native::Midi1PortNaming::ClassicCompatible;

            std::wstring errorMessage{};

            if (!native::config::SetDefaultMidi1PortNaming(value, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"PortNamingFailedFormat", winrt::hstring{ errorMessage }));

                RefreshGlobalSettings();

                co_return;
            }

            GlobalStatusText().Text(res::GetString(L"PortNamingChangedRestartNeeded"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the default MIDI 1.0 port naming.")

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRestartServiceClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            RestartServiceButton().IsEnabled(false);

            GlobalStatusText().Text(res::GetString(L"ServiceRestarting"));

            std::wstring errorMessage{};
            bool succeeded{ false };

            co_await native::RunOnBackgroundAsync([&succeeded, &errorMessage]()
                {
                    succeeded = native::config::RestartService(errorMessage);
                });

            if (m_closing)
            {
                co_return;
            }

            RestartServiceButton().IsEnabled(::winrt::midisettings::implementation::App::IsElevated());

            GlobalStatusText().Text(succeeded ?
                res::GetString(L"ServiceRestarted") :
                res::FormatString(L"ServiceRestartFailedFormat", winrt::hstring{ errorMessage }));

            // The watchers are bound to the old service instance, so they are rebuilt rather
            // than left listening to something that is gone.
            StopWatchers();

            m_serviceAvailable = false;

            CheckServiceHealthAsync();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to restart the MIDI service.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRestartElevatedClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (native::TryRelaunchElevated())
            {
                Close();
            }
            else
            {
                GlobalStatusText().Text(res::GetString(L"ElevationDeclined"));
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to relaunch with administrator rights.")
    }
}
