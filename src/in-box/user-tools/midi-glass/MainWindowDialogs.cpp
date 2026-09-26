// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The library's dialogs: making a layout from a template, and the three things the card menu
// cannot do without asking.

#include "pch.h"
#include "MainWindow.xaml.h"
#include "App.xaml.h"

#include "AppSettings.h"
#include "StringResources.h"
#include "LayoutStore.h"
#include "LayoutTemplates.h"
#include "ThumbnailRenderer.h"
#include "EndpointCatalog.h"

#include <filesystem>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        controls::TextBox MakeField(
            _In_ winrt::hstring const& header,
            _In_ winrt::hstring const& text,
            _In_ bool multiLine = false)
        {
            controls::TextBox box{};

            box.Header(box_value(header));
            box.Text(text);
            box.SelectAll();

            if (multiLine)
            {
                box.AcceptsReturn(true);
                box.TextWrapping(xaml::TextWrapping::Wrap);
                box.Height(96);
            }

            return box;
        }

        // The template list in the New layout dialog: a glyph, a name and a line saying what it
        // gives you, so nobody has to make one to find out.
        controls::ListView MakeTemplateList()
        {
            controls::ListView list{};

            list.SelectionMode(controls::ListViewSelectionMode::Single);
            list.Height(232);

            for (auto const& info : glass::LayoutTemplates())
            {
                controls::Grid row{};
                row.ColumnSpacing(12);
                row.Padding({ 0, 6, 0, 6 });

                controls::ColumnDefinition glyphColumn{};
                glyphColumn.Width({ 0, xaml::GridUnitType::Auto });

                controls::ColumnDefinition textColumn{};
                textColumn.Width({ 1, xaml::GridUnitType::Star });

                row.ColumnDefinitions().Append(glyphColumn);
                row.ColumnDefinitions().Append(textColumn);

                controls::FontIcon glyph{};
                glyph.Glyph(winrt::hstring{ std::wstring(1, info.Glyph) });
                glyph.FontSize(18);
                glyph.VerticalAlignment(xaml::VerticalAlignment::Center);
                controls::Grid::SetColumn(glyph, 0);

                controls::StackPanel text{};
                controls::Grid::SetColumn(text, 1);

                controls::TextBlock name{};
                name.Text(resources::GetString(info.NameResourceKey));
                name.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());

                controls::TextBlock detail{};
                detail.Text(resources::GetString(info.DescriptionResourceKey));
                detail.FontSize(12);
                detail.TextWrapping(xaml::TextWrapping::Wrap);
                detail.Foreground(xaml::Application::Current().Resources()
                    .Lookup(box_value(L"TextFillColorTertiaryBrush")).as<media::Brush>());

                text.Children().Append(name);
                text.Children().Append(detail);

                row.Children().Append(glyph);
                row.Children().Append(text);

                controls::ListViewItem item{};
                item.Content(row);
                item.Tag(box_value(static_cast<int32_t>(info.Kind)));

                // The whole row is one thing to a screen reader, not a glyph and two labels.
                xaml::Automation::AutomationProperties::SetName(item, winrt::hstring{
                    std::wstring{ resources::GetString(info.NameResourceKey) } + L". " +
                    std::wstring{ resources::GetString(info.DescriptionResourceKey) } });

                list.Items().Append(item);
            }

            list.SelectedIndex(0);

            return list;
        }
    }

    foundation::IAsyncAction MainWindow::ShowNewLayoutDialogAsync()
    {
        auto strong = get_strong();

        try
        {
            auto const endpoints = midiapp::EndpointCatalog::Current().Snapshot();

            auto nameBox = MakeField(
                resources::GetString(L"NewLayoutNameLabel"),
                resources::GetString(L"NewLayoutDefaultName"));

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

            auto templateList = MakeTemplateList();

            controls::TextBlock templateHeader{};
            templateHeader.Text(resources::GetString(L"NewLayoutTemplateLabel"));
            templateHeader.Margin({ 0, 4, 0, 0 });

            controls::StackPanel panel{};

            panel.Spacing(12);
            panel.Width(440);
            panel.Children().Append(nameBox);
            panel.Children().Append(picker);
            panel.Children().Append(templateHeader);
            panel.Children().Append(templateList);

            if (endpoints.empty())
            {
                controls::InfoBar warning{};

                warning.Severity(controls::InfoBarSeverity::Warning);
                warning.Title(resources::GetString(L"NewLayoutNoDevicesTitle"));
                warning.Message(resources::GetString(L"NewLayoutNoDevices"));
                warning.IsOpen(true);
                warning.IsClosable(false);

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

            auto kind = glass::LayoutTemplateKind::Mixer;

            if (auto const item = templateList.SelectedItem().try_as<controls::ListViewItem>())
            {
                kind = static_cast<glass::LayoutTemplateKind>(
                    winrt::unbox_value_or<int32_t>(item.Tag(), 0));
            }

            auto layoutName = std::wstring{ nameBox.Text() };

            if (layoutName.empty())
            {
                layoutName = std::wstring{ resources::GetString(L"NewLayoutDefaultName") };
            }

            auto document = glass::BuildLayoutFromTemplate(
                kind,
                layoutName,
                endpoint.Name,
                endpoint.BuildMatch(),
                midiapp::EndpointMatchMode::EndpointDeviceId);

            auto const folder = glass::LayoutsFolder();

            if (folder.empty())
            {
                co_return;
            }

            auto const path = glass::MakeUnusedLayoutPath(folder, layoutName);

            document.FilePath = path;

            if (glass::WriteLayoutFile(document, path))
            {
                ::midiglass::AppSettings::Current().RecordLayoutUse(path);

                RefreshLibrary();

                App::OpenRuntimeWindow(path);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to create a layout.")
    }

    _Use_decl_annotations_
    foundation::IAsyncAction MainWindow::RenameCardAsync(midiglass::LayoutCard card)
    {
        auto strong = get_strong();

        if (card == nullptr)
        {
            co_return;
        }

        try
        {
            auto nameBox = MakeField(
                resources::GetString(L"NewLayoutNameLabel"), card.DisplayName());

            controls::ContentDialog dialog{};

            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"RenameTitle")));
            dialog.Content(nameBox);
            dialog.PrimaryButtonText(resources::GetString(L"CommonSave"));
            dialog.CloseButtonText(resources::GetString(L"CommonCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const newName = std::wstring{ nameBox.Text() };

            if (newName.empty() || newName == std::wstring{ card.DisplayName() })
            {
                co_return;
            }

            auto const oldPath = std::wstring{ card.FilePath() };

            auto read = glass::ReadLayoutFile(oldPath);

            if (!read.Succeeded)
            {
                co_return;
            }

            read.Document.Name = newName;

            // The file follows the name, so the folder stays readable to somebody who does go
            // looking. A layout kept outside the layouts folder is left where it is.
            auto newPath = oldPath;

            if (glass::IsInLayoutsFolder(oldPath))
            {
                newPath = glass::MakeUnusedLayoutPath(glass::LayoutsFolder(), newName);
            }

            read.Document.FilePath = newPath;

            if (!glass::WriteLayoutFile(read.Document, newPath))
            {
                co_return;
            }

            if (newPath != oldPath)
            {
                std::error_code error{};
                std::filesystem::remove(oldPath, error);

                auto const oldCard = glass::ThumbnailPathForLayout(oldPath, glass::LargeThumbnailWidth);

                if (!oldCard.empty())
                {
                    std::filesystem::remove(oldCard, error);
                }
            }

            RefreshLibrary();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rename the layout.")
    }

    _Use_decl_annotations_
    foundation::IAsyncAction MainWindow::DescribeCardAsync(midiglass::LayoutCard card)
    {
        auto strong = get_strong();

        if (card == nullptr)
        {
            co_return;
        }

        try
        {
            auto descriptionBox = MakeField(
                resources::GetString(L"DescribeLabel"), card.Description(), true);

            descriptionBox.PlaceholderText(resources::GetString(L"DescribePlaceholder"));

            controls::ContentDialog dialog{};

            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(resources::FormatString(L"DescribeTitleFormat", card.DisplayName())));
            dialog.Content(descriptionBox);
            dialog.PrimaryButtonText(resources::GetString(L"CommonSave"));
            dialog.CloseButtonText(resources::GetString(L"CommonCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const description = std::wstring{ descriptionBox.Text() };

            if (EditLayoutFile(std::wstring{ card.FilePath() },
                [&description](glass::LayoutDocument& document) { document.Description = description; }))
            {
                RefreshLibrary();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the description.")
    }

    _Use_decl_annotations_
    foundation::IAsyncAction MainWindow::DeleteCardAsync(midiglass::LayoutCard card)
    {
        auto strong = get_strong();

        if (card == nullptr)
        {
            co_return;
        }

        try
        {
            controls::ContentDialog dialog{};

            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(resources::FormatString(L"DeleteTitleFormat", card.DisplayName())));
            dialog.Content(box_value(resources::GetString(L"DeleteBody")));
            dialog.PrimaryButtonText(resources::GetString(L"DeleteConfirm"));
            dialog.CloseButtonText(resources::GetString(L"CommonCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Close);

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const path = std::wstring{ card.FilePath() };

            std::error_code error{};

            if (!std::filesystem::remove(path, error) || error)
            {
                co_return;
            }

            // The card is derived, so losing it costs nothing and leaving it behind would show a
            // layout that is gone.
            auto const cardPath = glass::ThumbnailPathForLayout(path, glass::LargeThumbnailWidth);

            if (!cardPath.empty())
            {
                std::filesystem::remove(cardPath, error);
            }

            RefreshLibrary();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to delete the layout.")
    }
}
