// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Backing a layout up, putting one back, and moving one to another PC.
//
// All four commands are the same zip underneath. A layout is not one file - it names pictures
// that live beside it - so a backup that only carried the json would restore a surface full of
// empty rectangles.

#include "pch.h"
#include "MainWindow.xaml.h"
#include "App.xaml.h"

#include "StringResources.h"
#include "AppSettings.h"
#include "LayoutStore.h"
#include "LayoutPackage.h"

#include <shobjidl.h>
#include <shlobj_core.h>
#include <filesystem>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        // Somewhere to put a package, or one to read. The Win32 common item dialog, never
        // Windows.Storage.Pickers: this is an unpackaged desktop app.
        std::wstring PickPackagePath(
            _In_ HWND owner,
            _In_ bool saving,
            _In_ std::wstring const& suggestedName)
        {
            try
            {
                auto dialog = saving
                    ? winrt::create_instance<IFileDialog>(CLSID_FileSaveDialog)
                    : winrt::create_instance<IFileDialog>(CLSID_FileOpenDialog);

                if (dialog == nullptr)
                {
                    return {};
                }

                COMDLG_FILTERSPEC const filters[]
                {
                    { L"MIDI Glass layout package (*.zip)", L"*.zip" },
                };

                dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
                dialog->SetDefaultExtension(L"zip");

                dialog->SetTitle(resources::GetString(
                    saving ? L"PackageSaveTitle" : L"ImportOpenTitle").c_str());

                if (!suggestedName.empty())
                {
                    dialog->SetFileName(suggestedName.c_str());
                }

                if (FAILED(dialog->Show(owner)))
                {
                    return {};
                }

                winrt::com_ptr<IShellItem> item{};

                if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
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

        std::wstring DescribeSize(_In_ uint64_t bytes) noexcept
        {
            return resources::DescribeFileSize(bytes);
        }

        // A failure key the document layer handed back, as something a person can read.
        std::wstring Explain(_In_ std::wstring const& failureKey) noexcept
        {
            return std::wstring{ resources::GetString(
                failureKey.empty() ? L"PackageFailedWrite" : failureKey.c_str()) };
        }
    }

    // A plain "here is what happened". Not a status bar line: a backup nobody was told about is
    // a backup nobody trusts.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ShowNoticeAsync(
        std::wstring title,
        std::wstring body)
    {
        auto lifetime = get_strong();

        try
        {
            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(winrt::hstring{ title }));
            dialog.CloseButtonText(resources::GetString(L"DialogClose"));

            controls::TextBlock text{};

            text.Text(winrt::hstring{ body });
            text.TextWrapping(xaml::TextWrapping::Wrap);
            text.MaxWidth(420.0);

            dialog.Content(text);

            co_await dialog.ShowAsync();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show a notice.")
    }

    // A video on a layout can be larger than everything else this app has ever written put
    // together, and both the backup and the package carry it. Say what it will cost and let the
    // customer decide, rather than quietly writing two gigabytes or quietly refusing to.
    _Use_decl_annotations_
    winrt::Windows::Foundation::IAsyncOperation<int32_t> MainWindow::ConfirmPackageSizeAsync(
        std::wstring layoutFilePath,
        bool packaging)
    {
        auto lifetime = get_strong();

        auto const answer = [](PackageChoice choice) { return static_cast<int32_t>(choice); };

        auto const survey = glass::SurveyLayoutPackage(layoutFilePath);

        // Leaving the video out only makes sense for a backup. A package crossing to another PC
        // without its clips arrives as a surface full of empty rectangles.
        auto const canDropVideo = !packaging && survey.VideoCount > 0;

        if (survey.TooBig && !(canDropVideo && survey.FitsWithoutVideo))
        {
            ShowNoticeAsync(
                std::wstring{ resources::GetString(
                    packaging ? L"PackageFailedTitle" : L"BackupFailedTitle") },
                std::wstring{ resources::FormatString(
                    L"PackageTooBigFormat",
                    DescribeSize(survey.TotalBytes),
                    DescribeSize(glass::MaximumWritablePackageBytes)) });

            co_return answer(PackageChoice::Cancel);
        }

        if (!survey.TooBig && survey.TotalBytes <= glass::LargePackageBytes)
        {
            co_return answer(PackageChoice::Everything);
        }

        try
        {
            controls::StackPanel panel{};

            panel.Spacing(8.0);
            panel.MaxWidth(420.0);

            auto const paragraph = [&panel](std::wstring const& text, bool secondary)
                {
                    controls::TextBlock block{};

                    block.Text(winrt::hstring{ text });
                    block.TextWrapping(xaml::TextWrapping::Wrap);

                    if (secondary)
                    {
                        block.FontSize(12.0);
                        block.Foreground(xaml::Application::Current().Resources()
                            .Lookup(box_value(L"TextFillColorSecondaryBrush")).as<media::Brush>());
                    }

                    panel.Children().Append(block);
                };

            paragraph(
                std::wstring{ resources::FormatString(
                    L"PackageLargeBodyFormat",
                    DescribeSize(survey.TotalBytes),
                    std::to_wstring(survey.FileCount)) },
                false);

            if (!survey.LargestName.empty())
            {
                paragraph(
                    std::wstring{ resources::FormatString(
                        L"PackageLargestFormat",
                        survey.LargestName,
                        DescribeSize(survey.LargestBytes)) },
                    true);
            }

            if (canDropVideo)
            {
                paragraph(
                    std::wstring{ resources::FormatString(
                        L"BackupWithoutVideoCaptionFormat",
                        DescribeSize(survey.BytesWithoutVideo())) },
                    true);
            }
            else
            {
                paragraph(
                    std::wstring{ resources::GetString(
                        packaging ? L"PackageLargeCaption" : L"BackupLargeCaption") },
                    true);
            }

            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"PackageLargeTitle")));
            dialog.Content(panel);
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));

            if (canDropVideo)
            {
                // Without the video first and made the default: on this PC it is almost always
                // the right answer, and it is the one that does not cost anything.
                dialog.PrimaryButtonText(resources::GetString(L"BackupWithoutVideoAction"));
                dialog.SecondaryButtonText(resources::GetString(L"BackupLargeAction"));
                dialog.IsSecondaryButtonEnabled(!survey.TooBig);
            }
            else
            {
                dialog.PrimaryButtonText(resources::GetString(
                    packaging ? L"PackageLargeAction" : L"BackupLargeAction"));
            }

            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            switch (co_await dialog.ShowAsync())
            {
            case controls::ContentDialogResult::Primary:
                co_return answer(canDropVideo
                    ? PackageChoice::WithoutVideo
                    : PackageChoice::Everything);

            case controls::ContentDialogResult::Secondary:
                co_return answer(PackageChoice::Everything);

            default:
                co_return answer(PackageChoice::Cancel);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to ask about the size of the package.")

        co_return answer(PackageChoice::Cancel);
    }

    // ---------------------------------------------------------------- back up

    _Use_decl_annotations_
    void MainWindow::OnCardMenuBackUp(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        BackUpCardAsync(m_menuCard);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::BackUpCardAsync(midiglass::LayoutCard card)
    {
        auto lifetime = get_strong();

        if (card == nullptr || card.IsNewTile())
        {
            co_return;
        }

        try
        {
            auto const path = std::wstring{ card.FilePath() };

            auto const choice = static_cast<PackageChoice>(
                co_await ConfirmPackageSizeAsync(path, false));

            if (choice == PackageChoice::Cancel)
            {
                co_return;
            }

            auto const target = glass::NextBackupPath(path);

            if (target.empty())
            {
                ShowNoticeAsync(
                    std::wstring{ resources::GetString(L"BackupFailedTitle") },
                    std::wstring{ resources::GetString(L"PackageFailedWrite") });

                co_return;
            }

            auto const result = glass::WriteLayoutPackage(
                path, target, choice == PackageChoice::Everything);

            if (!result.Succeeded)
            {
                ShowNoticeAsync(
                    std::wstring{ resources::GetString(L"BackupFailedTitle") },
                    Explain(result.FailureKey));

                co_return;
            }

            ShowNoticeAsync(
                std::wstring{ resources::GetString(L"BackupDoneTitle") },
                std::wstring{ resources::FormatString(
                    choice == PackageChoice::WithoutVideo
                        ? L"BackupDoneWithoutVideoFormat"
                        : L"BackupDoneFormat",
                    std::filesystem::path{ result.Path }.filename().wstring(),
                    std::to_wstring(result.FileCount)) });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to back the layout up.")
    }

    // ---------------------------------------------------------------- restore

    _Use_decl_annotations_
    void MainWindow::OnCardMenuRestore(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        RestoreCardAsync(m_menuCard);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::RestoreCardAsync(midiglass::LayoutCard card)
    {
        auto lifetime = get_strong();

        if (card == nullptr || card.IsNewTile())
        {
            co_return;
        }

        try
        {
            auto const path = std::wstring{ card.FilePath() };
            auto const backups = glass::ListBackups(path);

            if (backups.empty())
            {
                ShowNoticeAsync(
                    std::wstring{ resources::GetString(L"RestoreNoneTitle") },
                    std::wstring{ resources::GetString(L"RestoreNoneBody") });

                co_return;
            }

            controls::StackPanel panel{};

            panel.Spacing(8.0);
            panel.Width(420.0);

            controls::TextBlock caption{};

            caption.Text(resources::GetString(L"RestorePickCaption"));
            caption.TextWrapping(xaml::TextWrapping::Wrap);
            caption.FontSize(12.0);
            caption.Foreground(xaml::Application::Current().Resources()
                .Lookup(box_value(L"TextFillColorSecondaryBrush")).as<media::Brush>());

            controls::ListView list{};

            list.SelectionMode(controls::ListViewSelectionMode::Single);
            list.MaxHeight(280.0);

            for (auto const& backup : backups)
            {
                controls::StackPanel row{};

                row.Spacing(1.0);

                controls::TextBlock name{};

                name.Text(winrt::hstring{
                    std::filesystem::path{ backup.Path }.filename().wstring() });
                name.FontSize(13.0);

                controls::TextBlock detail{};

                detail.Text(winrt::hstring{ DescribeSize(backup.Bytes) });
                detail.FontSize(11.0);
                detail.Foreground(xaml::Application::Current().Resources()
                    .Lookup(box_value(L"TextFillColorTertiaryBrush")).as<media::Brush>());

                row.Children().Append(name);
                row.Children().Append(detail);

                list.Items().Append(row);
            }

            list.SelectedIndex(0);

            panel.Children().Append(caption);
            panel.Children().Append(list);

            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"RestoreTitle")));
            dialog.Content(panel);
            dialog.PrimaryButtonText(resources::GetString(L"RestoreAction"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Close);

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const index = list.SelectedIndex();

            if (index < 0 || static_cast<size_t>(index) >= backups.size())
            {
                co_return;
            }

            auto const result = glass::RestoreLayoutFromBackup(
                backups[static_cast<size_t>(index)].Path, path);

            if (!result.Succeeded)
            {
                ShowNoticeAsync(
                    std::wstring{ resources::GetString(L"RestoreFailedTitle") },
                    Explain(result.FailureKey));

                co_return;
            }

            m_cardSignature.clear();
            RefreshLibrary();

            ShowNoticeAsync(
                std::wstring{ resources::GetString(L"RestoreDoneTitle") },
                std::wstring{ resources::GetString(L"RestoreDoneBody") });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to restore the layout.")
    }

    // ---------------------------------------------------------------- package

    _Use_decl_annotations_
    void MainWindow::OnCardMenuPackage(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        PackageCardAsync(m_menuCard);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::PackageCardAsync(midiglass::LayoutCard card)
    {
        auto lifetime = get_strong();

        if (card == nullptr || card.IsNewTile())
        {
            co_return;
        }

        try
        {
            auto const path = std::wstring{ card.FilePath() };

            // Asked before the save dialog, so nobody names a file and then finds out.
            if (static_cast<PackageChoice>(co_await ConfirmPackageSizeAsync(path, true)) ==
                PackageChoice::Cancel)
            {
                co_return;
            }

            auto suggested = std::filesystem::path{ path }.filename().wstring();

            if (auto const dot = suggested.find(L'.'); dot != std::wstring::npos)
            {
                suggested = suggested.substr(0, dot);
            }

            auto const target = PickPackagePath(
                m_chrome.WindowHandle(), true, suggested + glass::LayoutPackageExtension);

            if (target.empty())
            {
                co_return;
            }

            auto const result = glass::WriteLayoutPackage(path, target, true);

            if (!result.Succeeded)
            {
                ShowNoticeAsync(
                    std::wstring{ resources::GetString(L"PackageFailedTitle") },
                    Explain(result.FailureKey));

                co_return;
            }

            ShowNoticeAsync(
                std::wstring{ resources::GetString(L"PackageDoneTitle") },
                std::wstring{ resources::FormatString(
                    L"PackageDoneFormat",
                    std::filesystem::path{ result.Path }.filename().wstring(),
                    std::to_wstring(result.FileCount)) });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to package the layout.")
    }

    // ---------------------------------------------------------------- import

    _Use_decl_annotations_
    void MainWindow::OnImportPackageClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const source = PickPackagePath(m_chrome.WindowHandle(), false, {});

            if (source.empty())
            {
                return;
            }

            auto const result = glass::ReadLayoutPackage(source, glass::LayoutsFolder());

            if (!result.Succeeded)
            {
                ShowNoticeAsync(
                    std::wstring{ resources::GetString(L"ImportFailedTitle") },
                    Explain(result.FailureKey));

                return;
            }

            m_cardSignature.clear();
            RefreshLibrary();

            ShowNoticeAsync(
                std::wstring{ resources::GetString(L"ImportDoneTitle") },
                std::wstring{ resources::FormatString(
                    L"ImportDoneFormat",
                    std::filesystem::path{ result.Path }.filename().wstring(),
                    std::to_wstring(result.FileCount)) });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to import the layout package.")
    }

    // ---------------------------------------------------- the app-wide options

    _Use_decl_annotations_
    void MainWindow::OnShowBackupsFolderClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const folder = glass::BackupsFolderPath();

            if (folder.empty())
            {
                return;
            }

            auto const list = ::ILCreateFromPathW(folder.c_str());

            if (list != nullptr)
            {
                ::SHOpenFolderAndSelectItems(list, 0, nullptr, 0);
                ::ILFree(list);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the backups folder.")
    }

    _Use_decl_annotations_
    void MainWindow::OnKeepAwakeClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // It takes effect the next time a layout is opened. A layout already running keeps
            // whatever it was started with, because changing a machine's power behavior out
            // from under something that is on stage is not an improvement.
            ::midiglass::AppSettings::Current().KeepAwakeWhileRunning(
                KeepAwakeMenuItem().IsChecked());
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the keep awake setting.")
    }
}
