// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The background picture: choosing one, saying how it should fit, and taking it away again.
//
// The picture is stored as a bare file name beside the layout, never a path. That is what makes
// a layout something you can send somebody: the two files travel together. It also means a
// layout from a stranger cannot point this app at a file somewhere else on the PC.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "LayoutStore.h"

#include <winrt/Windows.Storage.FileProperties.h>

#include <shobjidl.h>
#include <filesystem>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr glass::BackgroundFit FitOrder[]
        {
            glass::BackgroundFit::Uniform,
            glass::BackgroundFit::Stretch,
            glass::BackgroundFit::Centered,
            glass::BackgroundFit::Tiled,
        };

        constexpr wchar_t const* FitKeys[]
        {
            L"BackgroundFitUniform", L"BackgroundFitStretch",
            L"BackgroundFitCentered", L"BackgroundFitTiled",
        };

        static_assert(std::size(FitOrder) == std::size(FitKeys));

        int32_t IndexOfFit(_In_ glass::BackgroundFit fit) noexcept
        {
            for (size_t index = 0; index < std::size(FitOrder); ++index)
            {
                if (FitOrder[index] == fit)
                {
                    return static_cast<int32_t>(index);
                }
            }

            return 0;
        }

        // A video has no picture of its own for an Image to show, so the preview is the frame
        // the shell shows for it. The token drops a frame that arrives after the choice moved on.
        winrt::fire_and_forget ShowVideoFrameAsync(
            controls::Image preview,
            std::wstring path,
            std::shared_ptr<uint32_t> token,
            uint32_t expected)
        {
            try
            {
                auto const file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path);
                auto const thumbnail = co_await file.GetThumbnailAsync(
                    winrt::Windows::Storage::FileProperties::ThumbnailMode::SingleItem, 320);

                if (thumbnail == nullptr || *token != expected)
                {
                    co_return;
                }

                media::Imaging::BitmapImage frame{};

                co_await frame.SetSourceAsync(thumbnail);

                if (*token == expected)
                {
                    preview.Source(frame);
                }
            }
            catch (...)
            {
                // No frame is not worth a word. The file name under the preview still says
                // what was chosen.
            }
        }
    }

    // The Win32 common item dialog, not Windows.Storage.Pickers: this is a desktop app and the
    // picker needs a window handle it can be modal to.
    _Use_decl_annotations_
    std::wstring EditorWindow::PickBackgroundImageFile(bool picturesOnly)
    {
        try
        {
            auto dialog = winrt::create_instance<IFileOpenDialog>(CLSID_FileOpenDialog);

            if (dialog == nullptr)
            {
                return {};
            }

            // The same files a control's picture takes. A background is copied beside the
            // layout the same way, so it is held to the same list.
            constexpr wchar_t PictureExtensions[] = L"*.png;*.jpg;*.jpeg";
            constexpr wchar_t VideoExtensions[] =
                L"*.mp4;*.m4v;*.mkv;*.webm;*.wmv;*.avi;*.mov;*.mpeg;*.mpg;*.m2v;*.asf";

            auto const both = std::wstring{ PictureExtensions } + L";" + VideoExtensions;

            auto const bothLabel = resources::FormatString(L"PictureFilterBothFormat", both);
            auto const pictureLabel =
                resources::FormatString(L"PictureFilterPicturesFormat", PictureExtensions);
            auto const videoLabel =
                resources::FormatString(L"PictureFilterVideoFormat", VideoExtensions);

            COMDLG_FILTERSPEC const filters[]
            {
                { bothLabel.c_str(), both.c_str() },
                { pictureLabel.c_str(), PictureExtensions },
                { videoLabel.c_str(), VideoExtensions },
            };

            // A theme's deck is drawn by a brush, which takes a picture and not a video.
            COMDLG_FILTERSPEC const pictureFilters[]
            {
                { pictureLabel.c_str(), PictureExtensions },
            };

            if (picturesOnly)
            {
                dialog->SetFileTypes(static_cast<UINT>(std::size(pictureFilters)), pictureFilters);
            }
            else
            {
                dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
            }
            dialog->SetTitle(resources::GetString(L"BackgroundOpenTitle").c_str());

            if (FAILED(dialog->Show(m_chrome.WindowHandle())))
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
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to choose a background picture.")

        return {};
    }

    _Use_decl_annotations_
    void EditorWindow::OnBackgroundClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowBackgroundDialog();
    }

    winrt::fire_and_forget EditorWindow::ShowBackgroundDialog()
    {
        auto lifetime = get_strong();

        try
        {
            if (m_openDialog != nullptr)
            {
                co_return;
            }

            // Edited on a copy, so a canceled dialog leaves nothing behind. The chosen path is
            // the full one until Save; only then is it copied and reduced to a file name.
            auto chosenPath = std::make_shared<std::wstring>();
            auto chosenName = std::make_shared<std::wstring>(m_editor.Document().BackgroundImage);

            controls::StackPanel panel{};
            panel.Spacing(10.0);
            panel.Width(420.0);

            controls::Image preview{};
            preview.Height(150.0);
            preview.Stretch(media::Stretch::Uniform);
            preview.HorizontalAlignment(xaml::HorizontalAlignment::Center);

            controls::Border previewFrame{};
            previewFrame.Height(150.0);
            previewFrame.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6.0));
            previewFrame.Background(
                xaml::Application::Current().Resources()
                    .Lookup(box_value(L"SubtleFillColorSecondaryBrush")).as<media::Brush>());
            previewFrame.Child(preview);

            controls::TextBlock fileText{};
            fileText.FontSize(12.0);
            fileText.TextWrapping(xaml::TextWrapping::Wrap);

            controls::TextBlock copyNote{};
            copyNote.FontSize(11.0);
            copyNote.TextWrapping(xaml::TextWrapping::Wrap);
            copyNote.Visibility(xaml::Visibility::Collapsed);
            copyNote.Foreground(
                xaml::Application::Current().Resources()
                    .Lookup(box_value(L"SystemFillColorCautionBrush")).as<media::Brush>());

            controls::ComboBox fitCombo{};
            fitCombo.Header(box_value(resources::GetString(L"BackgroundFitHeader")));
            fitCombo.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const* const key : FitKeys)
            {
                fitCombo.Items().Append(box_value(resources::GetString(key)));
            }

            fitCombo.SelectedIndex(IndexOfFit(m_editor.Document().BackgroundFitMode));

            controls::Button chooseButton{};
            chooseButton.Content(box_value(resources::GetString(L"BackgroundChoose")));

            controls::Button clearButton{};
            clearButton.Content(box_value(resources::GetString(L"BackgroundClear")));

            controls::StackPanel buttonRow{};
            buttonRow.Orientation(controls::Orientation::Horizontal);
            buttonRow.Spacing(8.0);
            buttonRow.Children().Append(chooseButton);
            buttonRow.Children().Append(clearButton);

            auto const layoutPath = m_filePath;
            auto const previewToken = std::make_shared<uint32_t>(0);

            // Shows whatever is chosen right now: the file already beside the layout, or one
            // picked from somewhere else that has not been copied yet.
            auto const refresh = [=]()
                {
                    auto const source = chosenPath->empty()
                        ? glass::BackgroundImagePath(m_editor.Document())
                        : *chosenPath;

                    auto const showing = !source.empty();
                    auto const expected = ++(*previewToken);

                    preview.Source(nullptr);

                    if (showing && glass::IsVideoFileName(source))
                    {
                        ShowVideoFrameAsync(preview, source, previewToken, expected);
                    }
                    else if (showing)
                    {
                        try
                        {
                            media::Imaging::BitmapImage bitmap{};
                            bitmap.UriSource(foundation::Uri{ winrt::hstring{ source } });
                            preview.Source(bitmap);
                        }
                        catch (...)
                        {
                            preview.Source(nullptr);
                        }
                    }

                    winrt::hstring caption{ resources::GetString(L"BackgroundNone") };

                    if (showing)
                    {
                        std::filesystem::path const file{ source };
                        caption = winrt::hstring{ file.filename().wstring() };
                    }

                    fileText.Text(caption);

                    // Said before Save, not after. Copying a file into the customer's folder is
                    // not something to discover afterwards.
                    auto const willCopy = !chosenPath->empty() &&
                        glass::BackgroundImageNeedsCopying(*chosenPath, layoutPath);

                    copyNote.Visibility(willCopy
                        ? xaml::Visibility::Visible
                        : xaml::Visibility::Collapsed);

                    if (willCopy)
                    {
                        std::filesystem::path const layoutFile{ layoutPath };

                        copyNote.Text(resources::FormatString(
                            L"BackgroundWillCopyFormat", layoutFile.parent_path().wstring()));
                    }

                    fitCombo.IsEnabled(showing);
                    clearButton.IsEnabled(showing);
                };

            chooseButton.Click([=](auto&&, auto&&)
                {
                    if (auto picked = PickBackgroundImageFile(false); !picked.empty())
                    {
                        *chosenPath = picked;
                        chosenName->clear();

                        refresh();
                    }
                });

            clearButton.Click([=](auto&&, auto&&)
                {
                    chosenPath->clear();
                    chosenName->clear();
                    ++(*previewToken);

                    preview.Source(nullptr);
                    fileText.Text(resources::GetString(L"BackgroundNone"));
                    copyNote.Visibility(xaml::Visibility::Collapsed);
                    fitCombo.IsEnabled(false);
                    clearButton.IsEnabled(false);
                });

            panel.Children().Append(previewFrame);
            panel.Children().Append(fileText);
            panel.Children().Append(buttonRow);
            panel.Children().Append(fitCombo);
            panel.Children().Append(copyNote);

            refresh();

            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"BackgroundDialogTitle")));
            dialog.PrimaryButtonText(resources::GetString(L"DialogSave"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);
            dialog.Content(panel);

            m_openDialog = dialog.ShowAsync();
            auto const result = co_await m_openDialog;
            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto name = *chosenName;

            if (!chosenPath->empty())
            {
                // A video can be larger than everything else the customer has made with this
                // app, so the size is said before the copy, the same as for a control's picture.
                if (!co_await ConfirmPictureSizeAsync(*chosenPath))
                {
                    co_return;
                }

                // Copying happens on Save, not on choosing, so a canceled dialog never leaves a
                // file behind in the customer's folder.
                name = glass::CopyBackgroundImageBeside(*chosenPath, layoutPath);

                if (name.empty())
                {
                    co_return;
                }
            }

            auto const fitIndex = fitCombo.SelectedIndex();

            auto const fit = (fitIndex >= 0 && static_cast<size_t>(fitIndex) < std::size(FitOrder))
                ? FitOrder[static_cast<size_t>(fitIndex)]
                : glass::BackgroundFit::Uniform;

            if (m_editor.SetBackgroundImage(name, fit))
            {
                RebuildSurface();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the background picture.")

        m_openDialog = nullptr;
    }
}
