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
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;

namespace winrt::midisettings::implementation
{
    namespace
    {
        // The Win32 dialog for the same reason as the save dialog: the WinRT picker's completion
        // never resumes when it is raised over an open ContentDialog, and this one always is.
        std::wstring ShowOpenConfigDialog(
            _In_ HWND const owner,
            _In_ std::wstring const& startFolder) noexcept
        {
            try
            {
                winrt::com_ptr<IFileOpenDialog> dialog{};

                if (FAILED(::CoCreateInstance(
                    CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
                {
                    return {};
                }

                auto const configFilterName = res::GetString(L"ConfigFileDialogFilterName");
                auto const allFilesFilterName = res::GetString(L"AllFilesDialogFilterName");

                COMDLG_FILTERSPEC const filters[]
                {
                    { configFilterName.c_str(), L"*.midiconfig.json" },
                    { allFilesFilterName.c_str(), L"*.*" },
                };

                LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
                LOG_IF_FAILED(dialog->SetOptions(FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM));

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

        // The customer's own short date and time, so two backups a few minutes apart read apart.
        std::pair<std::wstring, std::wstring> LocalDateAndTime(_In_ FILETIME const& written)
        {
            SYSTEMTIME utc{};
            SYSTEMTIME local{};

            if (!::FileTimeToSystemTime(&written, &utc) ||
                !::SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local))
            {
                return {};
            }

            wchar_t dateText[80]{};
            wchar_t timeText[80]{};

            if (::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr, dateText, ARRAYSIZE(dateText), nullptr) == 0 ||
                ::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, timeText, ARRAYSIZE(timeText)) == 0)
            {
                return {};
            }

            return { dateText, timeText };
        }

        std::wstring FailureMessage(
            _In_ native::config::ConfigFileOperationResult const& result,
            _In_ bool const backingUpOnly)
        {
            using native::config::ConfigFileError;

            std::wstring_view key{ L"ConfigErrorWriteFailed" };

            switch (result.Error)
            {
            case ConfigFileError::NoFolder:             key = L"ConfigErrorNoFolder"; break;
            case ConfigFileError::NameNotUsable:        key = L"ConfigErrorNameNotUsable"; break;
            case ConfigFileError::NotFound:             key = L"ConfigErrorNotFound"; break;
            case ConfigFileError::Empty:                key = L"ConfigErrorEmpty"; break;
            case ConfigFileError::TooLarge:             key = L"ConfigErrorTooLarge"; break;
            case ConfigFileError::NotAConfiguration:    key = L"ConfigErrorNotAConfiguration"; break;
            case ConfigFileError::ReadFailed:           key = L"ConfigErrorReadFailed"; break;
            case ConfigFileError::BackupFailed:
                key = backingUpOnly ? L"ConfigErrorBackupFailed" : L"ConfigErrorNotReplaced";
                break;
            default:
                break;
            }

            std::wstring message{ res::GetString(key) };

            // the write failed after the old version was saved, so it can still come back
            if (!result.BackupFileName.empty())
            {
                message += L" ";
                message += res::GetString(L"ConfigReplacedBackedUp");
            }

            auto const systemText = native::config::SystemErrorText(result.SystemError);

            if (!systemText.empty())
            {
                message += L" ";
                message += systemText;
            }

            return message;
        }
    }

    // In the configuration card, so the result of a button there shows next to it rather than
    // below the fold of the dialog.
    _Use_decl_annotations_
    void MainWindow::ShowConfigFileStatus(winrt::hstring const& message) noexcept
    {
        try
        {
            // visible first, so the live region announces the new text
            ConfigFileStatusText().Visibility(message.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            ConfigFileStatusText().Text(message);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the configuration status.")
    }

    void MainWindow::RefreshConfigBackups() noexcept
    {
        try
        {
            auto const elevated = ::winrt::midisettings::implementation::App::IsElevated();

            BackUpConfigButton().IsEnabled(elevated && !native::config::CurrentFullPath().empty());
            AddConfigButton().IsEnabled(elevated);

            auto const backups = native::config::EnumerateBackups();

            m_configBackupChoices.Clear();

            for (auto const& backup : backups)
            {
                auto const [writtenDate, writtenTime] = LocalDateAndTime(backup.WrittenAt);

                m_configBackupChoices.Append(winrt::make<implementation::ConfigFileChoice>(
                    res::FormatString(L"ConfigBackupChoiceFormat", backup.ConfigName, writtenDate, writtenTime),
                    winrt::hstring{ backup.BackupFileName }));
            }

            ConfigBackupComboBox().SelectedIndex(backups.empty() ? -1 : 0);
            ConfigBackupComboBox().IsEnabled(!backups.empty());
            RestoreConfigBackupButton().IsEnabled(elevated && !backups.empty());
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to list the configuration backups.")
    }

    // An added or restored configuration becomes the active one, the same as a new one does,
    // because that is what loading a configuration is for.
    _Use_decl_annotations_
    void MainWindow::FinishPutInPlace(
        ::midisettings::config::ConfigFileOperationResult const& result,
        std::wstring_view const doneKey) noexcept
    {
        try
        {
            if (result.Error != native::config::ConfigFileError::None)
            {
                // a backup may still have been made, and the list should show it
                RefreshGlobalSettings();
                ShowConfigFileStatus(winrt::hstring{ FailureMessage(result, false) });
                return;
            }

            std::wstring errorMessage{};

            auto const madeActive = native::config::SetCurrentFileName(result.ConfigFileName, errorMessage);

            RefreshGlobalSettings();

            auto const configName = native::config::ReadConfigName(
                native::config::FolderPath() + result.ConfigFileName);

            std::wstring message{ res::FormatString(
                result.Unchanged ? std::wstring_view{ L"ConfigAlreadyHereFormat" } : doneKey, configName) };

            if (!result.BackupFileName.empty())
            {
                message += L" ";
                message += res::GetString(L"ConfigReplacedBackedUp");
            }

            message += L" ";
            message += madeActive ?
                std::wstring{ res::GetString(L"ConfigActiveRestartNeeded") } :
                std::wstring{ res::FormatString(L"ConfigNotMadeActiveFormat", errorMessage) };

            ShowConfigFileStatus(winrt::hstring{ message });
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to finish adding the configuration.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnBackUpConfigClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const configFileName = native::config::CurrentFileName();

            if (configFileName.empty() || native::config::CurrentFullPath().empty())
            {
                ShowConfigFileStatus(res::GetString(L"ConfigFileNoneActive"));
                co_return;
            }

            BackUpConfigButton().IsEnabled(false);

            native::config::ConfigFileOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result, configFileName]()
                {
                    result = native::config::BackUpFile(configFileName);
                });

            RefreshGlobalSettings();

            if (result.Error != native::config::ConfigFileError::None)
            {
                ShowConfigFileStatus(winrt::hstring{ FailureMessage(result, true) });
                co_return;
            }

            ShowConfigFileStatus(res::FormatString(
                L"ConfigBackedUpFormat",
                native::config::ReadConfigName(native::config::FolderPath() + configFileName)));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to back up the configuration file.")

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnAddConfigFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const source = ShowOpenConfigDialog(
                WindowHandle(), native::AppSettings::Current().LastConfigCopyFolder());

            if (source.empty())
            {
                co_return;
            }

            native::AppSettings::Current().LastConfigCopyFolder(
                std::filesystem::path{ source }.parent_path().wstring());

            AddConfigButton().IsEnabled(false);
            RestoreConfigBackupButton().IsEnabled(false);

            native::config::ConfigFileOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result, source]()
                {
                    result = native::config::AddFromFile(source);
                });

            FinishPutInPlace(result, L"ConfigAddedFormat");
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to add the configuration file.")

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRestoreConfigBackupClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const selected = ConfigBackupComboBox().SelectedItem()
                .try_as<midisettings::ConfigFileChoice>();

            if (selected == nullptr)
            {
                ShowConfigFileStatus(res::GetString(L"ConfigNoBackupSelected"));
                co_return;
            }

            std::wstring const backupFileName{ selected.FileName() };

            AddConfigButton().IsEnabled(false);
            RestoreConfigBackupButton().IsEnabled(false);

            native::config::ConfigFileOperationResult result{};

            co_await native::RunOnBackgroundAsync([&result, backupFileName]()
                {
                    result = native::config::RestoreBackup(backupFileName);
                });

            FinishPutInPlace(result, L"ConfigRestoredFormat");
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to restore the configuration backup.")

        co_return;
    }
}
