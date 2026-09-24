// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The library half of the main window: the cards, their thumbnails, making a starter layout and
// running one.

#include "pch.h"
#include "MainWindow.xaml.h"
#include "App.xaml.h"

#include "StringResources.h"
#include "LayoutStore.h"
#include "StarterLayout.h"
#include "ThemeStore.h"
#include "ThumbnailLayout.h"
#include "ThumbnailRenderer.h"
#include "EndpointCatalog.h"

#include <filesystem>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        // Regenerated when the layout is newer than its card. Deleting the whole cache must never
        // lose anything, so a missing card is simply drawn again.
        bool CardIsStale(
            _In_ std::wstring const& layoutPath,
            _In_ std::wstring const& cardPath) noexcept
        {
            try
            {
                std::error_code error{};

                if (!std::filesystem::exists(cardPath, error) || error)
                {
                    return true;
                }

                auto const layoutTime = std::filesystem::last_write_time(layoutPath, error);

                if (error)
                {
                    return true;
                }

                auto const cardTime = std::filesystem::last_write_time(cardPath, error);

                return error || cardTime < layoutTime;
            }
            catch (...)
            {
                return true;
            }
        }

        std::wstring FallbackName(_In_ std::wstring const& filePath) noexcept
        {
            try
            {
                auto name = std::filesystem::path{ filePath }.filename().wstring();

                auto const suffix = std::wstring{ glass::LayoutFileExtension };

                if (name.size() > suffix.size() &&
                    name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
                {
                    name.erase(name.size() - suffix.size());
                }

                return name;
            }
            catch (...)
            {
                return filePath;
            }
        }

        media::ImageSource LoadCard(_In_ std::wstring const& cardPath) noexcept
        {
            try
            {
                std::error_code error{};

                if (cardPath.empty() || !std::filesystem::exists(cardPath, error) || error)
                {
                    return nullptr;
                }

                media::Imaging::BitmapImage bitmap{};

                bitmap.DecodePixelWidth(glass::LargeThumbnailWidth);
                bitmap.UriSource(foundation::Uri{ L"file:///" + winrt::hstring{ cardPath } });

                return bitmap;
            }
            catch (...)
            {
                return nullptr;
            }
        }
    }

    void MainWindow::RefreshLibrary()
    {
        if (m_refreshing)
        {
            return;
        }

        m_refreshing = true;

        auto weak = get_weak();

        // Reading every layout and drawing every missing card is disk work, so it never happens
        // on the UI thread.
        std::thread([weak]()
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);

                std::vector<::midiglass::LayoutCardData> cards{};

                try
                {
                    auto const themes = glass::AllThemes();

                    for (auto const& path : glass::ListLayoutFiles())
                    {
                        auto const read = glass::ReadLayoutFile(path);

                        ::midiglass::LayoutCardData card{};

                        card.FilePath = path;

                        if (!read.Succeeded)
                        {
                            card.DisplayName = FallbackName(path);
                            card.DetailText = std::wstring{
                                resources::GetString(L"LibraryUnreadable") };

                            cards.push_back(std::move(card));
                            continue;
                        }

                        auto const& document = read.Document;

                        card.DisplayName = document.Name.empty() ? FallbackName(path) : document.Name;
                        card.Description = document.Description;
                        card.IsImported = document.IsImported;

                        card.DetailText = std::wstring{ resources::FormatString(
                            L"LibraryDetailFormat",
                            static_cast<int32_t>(document.ControlCount()),
                            document.PageWidth,
                            document.PageHeight,
                            document.ThemeName.empty()
                                ? std::wstring{ L"Studio Dark" }
                                : document.ThemeName) };

                        auto const cardPath = glass::ThumbnailPathForLayout(path, glass::LargeThumbnailWidth);

                        if (!cardPath.empty() && CardIsStale(path, cardPath))
                        {
                            auto const* theme = themes.empty() ? nullptr : &themes[0];

                            for (auto const& candidate : themes)
                            {
                                if (candidate.Name == document.ThemeName)
                                {
                                    theme = &candidate;
                                    break;
                                }
                            }

                            if (theme != nullptr)
                            {
                                glass::RenderThumbnailToFile(
                                    glass::PlanThumbnail(
                                        document,
                                        *theme,
                                        glass::LargeThumbnailWidth,
                                        glass::LargeThumbnailHeight),
                                    cardPath);
                            }
                        }

                        cards.push_back(std::move(card));
                    }
                }
                catch (...)
                {
                }

                auto strong = weak.get();

                if (strong != nullptr && strong->m_dispatcher != nullptr)
                {
                    strong->m_dispatcher.TryEnqueue([weak, cards]()
                        {
                            if (auto inner = weak.get())
                            {
                                inner->ApplyCards(cards);
                                inner->m_refreshing = false;
                            }
                        });
                }

                winrt::uninit_apartment();
            }).detach();
    }

    _Use_decl_annotations_
    void MainWindow::ApplyCards(std::vector<::midiglass::LayoutCardData> const& cards)
    {
        try
        {
            m_cards.Clear();

            for (auto const& data : cards)
            {
                auto card = winrt::make_self<LayoutCard>();

                card->Update(data);
                card->Thumbnail(LoadCard(
                    glass::ThumbnailPathForLayout(data.FilePath, glass::LargeThumbnailWidth)));

                m_cards.Append(*card);
            }

            auto const empty = m_cards.Size() == 0;

            EmptyLibraryPanel().Visibility(empty ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            LayoutGrid().Visibility(empty ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the layout library.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRefreshClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        RefreshLibrary();
    }

    _Use_decl_annotations_
    void MainWindow::OnOpenFolderClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const folder = glass::LayoutsFolder();

            if (folder.empty())
            {
                return;
            }

            ::ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open the layouts folder.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLayoutItemClick(
        foundation::IInspectable const& sender,
        controls::ItemClickEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            if (auto const card = args.ClickedItem().try_as<midiglass::LayoutCard>())
            {
                App::OpenRuntimeWindow(std::wstring{ card.FilePath() });
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to run the layout.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRunLayoutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const button = sender.try_as<controls::Button>();

            if (button == nullptr)
            {
                return;
            }

            auto const path = winrt::unbox_value_or<winrt::hstring>(button.Tag(), L"");

            if (!path.empty())
            {
                App::OpenRuntimeWindow(std::wstring{ path });
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to run the layout.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNewLayoutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowNewLayoutDialogAsync();
    }

    foundation::IAsyncAction MainWindow::ShowNewLayoutDialogAsync()
    {
        auto strong = get_strong();

        try
        {
            auto const endpoints = midiapp::EndpointCatalog::Current().Snapshot();

            controls::ComboBox picker{};

            picker.Header(box_value(resources::GetString(L"NewLayoutDeviceLabel")));
            picker.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const& endpoint : endpoints)
            {
                picker.Items().Append(box_value(winrt::hstring{ endpoint.Name }));
            }

            if (!endpoints.empty())
            {
                picker.SelectedIndex(0);
            }

            controls::TextBox nameBox{};

            nameBox.Header(box_value(resources::GetString(L"NewLayoutNameLabel")));
            nameBox.Text(resources::GetString(L"NewLayoutDefaultName"));

            controls::StackPanel panel{};

            panel.Spacing(12);
            panel.Children().Append(nameBox);
            panel.Children().Append(picker);

            if (endpoints.empty())
            {
                controls::TextBlock warning{};

                warning.Text(resources::GetString(L"NewLayoutNoDevices"));
                warning.TextWrapping(xaml::TextWrapping::Wrap);

                panel.Children().Append(warning);
            }

            controls::ContentDialog dialog{};

            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"NewLayoutTitle")));
            dialog.Content(panel);
            dialog.PrimaryButtonText(resources::GetString(L"NewLayoutCreate"));
            dialog.CloseButtonText(resources::GetString(L"CommonCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);
            dialog.IsPrimaryButtonEnabled(!endpoints.empty());

            auto const result = co_await dialog.ShowAsync();

            if (result != controls::ContentDialogResult::Primary || endpoints.empty())
            {
                co_return;
            }

            auto const selected = picker.SelectedIndex();

            if (selected < 0 || static_cast<size_t>(selected) >= endpoints.size())
            {
                co_return;
            }

            auto const& endpoint = endpoints[static_cast<size_t>(selected)];

            auto layoutName = std::wstring{ nameBox.Text() };

            if (layoutName.empty())
            {
                layoutName = std::wstring{ resources::GetString(L"NewLayoutDefaultName") };
            }

            auto document = glass::BuildStarterLayout(
                layoutName,
                endpoint.Name,
                endpoint.BuildMatch(),
                midiapp::EndpointMatchMode::EndpointDeviceId);

            auto const folder = glass::LayoutsFolder();

            if (folder.empty())
            {
                co_return;
            }

            // A file name is not a layout name: anything Windows will not take in a path is
            // replaced rather than refused, so nobody has to guess which character was the
            // problem.
            std::wstring fileName{};

            for (auto const character : layoutName)
            {
                fileName += (::wcschr(L"\\/:*?\"<>|", character) != nullptr) ? L'-' : character;
            }

            auto path = folder + L"\\" + fileName + glass::LayoutFileExtension;

            for (int32_t attempt = 2; std::filesystem::exists(path) && attempt < 100; ++attempt)
            {
                path = folder + L"\\" + fileName + L" " + std::to_wstring(attempt) +
                    glass::LayoutFileExtension;
            }

            document.FilePath = path;

            if (glass::WriteLayoutFile(document, path))
            {
                RefreshLibrary();
                App::OpenRuntimeWindow(path);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to create a starter layout.")
    }
}
