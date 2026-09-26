// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Layout settings: the pages of a layout and the device table it points at.
//
// This is the screen that makes a layout portable. A control never names hardware; it names an
// entry in the table, and the table is what gets re-pointed when the layout moves to another PC
// or the keyboard moves to another port.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "EndpointCatalog.h"
#include "EndpointImageAssets.h"
#include "MidiEndpointHelpers.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;
        namespace text = ::winrt::Windows::UI::Text;

        media::SolidColorBrush BrushFromKey(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(box_value(key)).as<media::SolidColorBrush>();
        }

        controls::TextBlock MakeText(
            _In_ winrt::hstring const& text,
            _In_ double fontSize,
            _In_ wchar_t const* brushKey)
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(fontSize);
            block.VerticalAlignment(xaml::VerticalAlignment::Center);
            block.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            block.Foreground(BrushFromKey(brushKey));

            return block;
        }

        // The Ready / Not here right now pill from the comp.
        controls::Border MakeStatusChip(_In_ bool ready)
        {
            controls::Border chip{};

            chip.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(9.0));
            chip.Padding(xaml::ThicknessHelper::FromLengths(8.0, 1.0, 9.0, 2.0));
            chip.VerticalAlignment(xaml::VerticalAlignment::Center);

            chip.Background(BrushFromKey(ready
                ? L"SystemFillColorSuccessBackgroundBrush"
                : L"SystemFillColorCriticalBackgroundBrush"));

            controls::StackPanel row{};
            row.Orientation(controls::Orientation::Horizontal);
            row.Spacing(5.0);

            controls::FontIcon glyph{};
            glyph.Glyph(ready ? L"\uE73E" : L"\uE711");
            glyph.FontSize(10.0);
            glyph.Foreground(BrushFromKey(ready
                ? L"SystemFillColorSuccessBrush"
                : L"SystemFillColorCriticalBrush"));

            auto label = MakeText(
                resources::GetString(ready ? L"DeviceReady" : L"DeviceNotHere"),
                11.0,
                ready ? L"SystemFillColorSuccessBrush" : L"SystemFillColorCriticalBrush");

            row.Children().Append(glyph);
            row.Children().Append(label);
            chip.Child(row);

            return chip;
        }

        controls::Button MakeMenuButton()
        {
            controls::Button button{};

            controls::FontIcon glyph{};
            glyph.Glyph(L"\uE712");
            glyph.FontSize(13.0);

            button.Content(glyph);
            button.Width(30.0);
            button.Height(26.0);
            button.Padding(xaml::ThicknessHelper::FromUniformLength(0.0));
            button.VerticalAlignment(xaml::VerticalAlignment::Center);
            button.Background(nullptr);
            button.BorderThickness(xaml::ThicknessHelper::FromUniformLength(0.0));

            automation::AutomationProperties::SetName(button, resources::GetString(L"MoreActions"));

            return button;
        }

        wchar_t const* MatchModeResourceKey(_In_ midiapp::EndpointMatchMode mode) noexcept
        {
            switch (mode)
            {
            case midiapp::EndpointMatchMode::UsbVendorAndProduct: return L"MatchModeUsbModel";
            case midiapp::EndpointMatchMode::EndpointName:        return L"MatchModeName";
            default:                                             return L"MatchModeExactDevice";
            }
        }

        constexpr midiapp::EndpointMatchMode MatchModeOrder[]
        {
            midiapp::EndpointMatchMode::EndpointDeviceId,
            midiapp::EndpointMatchMode::EndpointName,
            midiapp::EndpointMatchMode::UsbVendorAndProduct,
        };

        constexpr wchar_t const* MatchModeKeys[]
        {
            L"MatchModeExactDevice", L"MatchModeName", L"MatchModeUsbModel",
        };

        static_assert(std::size(MatchModeOrder) == std::size(MatchModeKeys));

        // The device picker's panel width.
        constexpr double DevicePickerWidth = 560.0;

        // Every endpoint row is this tall whatever it has to say, so the list reads as a list
        // rather than as a ragged column.
        constexpr double DevicePickerRowHeight = 64.0;

        // How wide a line in an endpoint row may be before it is trimmed.
        //
        // It is a number rather than a layout rule on purpose. A ListView arranges its rows at
        // their desired width and lets the scroll viewer clip what does not fit, so a long
        // description is cut through the middle of a letter with no ellipsis however the
        // containers are aligned. Bounding the text block itself is the only thing that makes
        // the text engine trim rather than the layout clip.
        constexpr double DevicePickerLineWidth = 400.0;

        // A row line that is trimmed rather than clipped when it runs out of room.
        controls::TextBlock MakeRowLine(
            _In_ winrt::hstring const& text,
            _In_ double fontSize,
            _In_ wchar_t const* brushKey)
        {
            auto block = MakeText(text, fontSize, brushKey);

            block.MaxWidth(DevicePickerLineWidth);
            block.HorizontalAlignment(xaml::HorizontalAlignment::Left);

            return block;
        }

        // The second line of an endpoint in the picker: what the thing is, and what it can do.
        // A transport code is an implementation detail, and a plain group count does not say
        // which way the messages go, which is the only thing the count is useful for.
        //
        // The two are separate lines rather than one: a description long enough to be trimmed
        // would otherwise take the group counts off the end of the row with it, and the counts
        // are the part somebody is choosing on.
        std::wstring DescribeEndpointGroups(_In_ midiapp::LiveEndpoint const& endpoint)
        {
            auto const sources = endpoint.SourceGroupCount();
            auto const destinations = endpoint.DestinationGroupCount();

            std::wstring groups{};

            if (sources > 0)
            {
                groups = resources::FormatString(
                    sources == 1 ? L"EndpointOneSourceGroupFormat" : L"EndpointSourceGroupsFormat",
                    std::to_wstring(sources));
            }

            if (destinations > 0)
            {
                if (!groups.empty()) { groups += L", "; }

                groups += resources::FormatString(
                    destinations == 1 ? L"EndpointOneDestinationGroupFormat" : L"EndpointDestinationGroupsFormat",
                    std::to_wstring(destinations));
            }

            return groups;
        }

        std::wstring DescribeEndpoint(_In_ midiapp::LiveEndpoint const& endpoint)
        {
            std::wstring text{ endpoint.Description };

            auto const groups = DescribeEndpointGroups(endpoint);

            if (!groups.empty())
            {
                if (!text.empty()) { text += L" \u00b7 "; }
                text += groups;
            }

            return text;
        }

        // BitmapImage cannot render SVG and the shipped default endpoint art is SVG, so the
        // decoder is chosen by extension, the same way the Settings app does it.
        media::ImageSource LoadEndpointImage(_In_ std::wstring const& path, _In_ int32_t pixelHeight) noexcept
        {
            if (path.empty())
            {
                return nullptr;
            }

            try
            {
                foundation::Uri const uri{ L"file:///" + winrt::hstring{ path } };

                if (midiapp::EndpointImageAssets::IsScalableVector(path))
                {
                    media::Imaging::SvgImageSource source{};

                    source.RasterizePixelHeight(pixelHeight);
                    source.UriSource(uri);

                    return source;
                }

                media::Imaging::BitmapImage bitmap{};

                bitmap.DecodePixelHeight(pixelHeight);
                bitmap.UriSource(uri);

                return bitmap;
            }
            catch (...)
            {
                return nullptr;
            }
        }

        controls::Image MakeEndpointImage(
            _In_ std::wstring const& customPath,
            _In_ std::wstring const& transportCode,
            _In_ double size)
        {
            controls::Image image{};

            image.Width(size);
            image.Height(size);
            image.Stretch(media::Stretch::Uniform);
            image.VerticalAlignment(xaml::VerticalAlignment::Center);

            auto const path = midiapp::ResolveEndpointImageOrDefault(
                winrt::hstring{ customPath }, transportCode);

            image.Source(LoadEndpointImage(std::wstring{ path }, static_cast<int32_t>(size * 2.0)));

            return image;
        }
    }

    // ------------------------------------------------------------------ showing the screen

    _Use_decl_annotations_
    void EditorWindow::OnLayoutSettingsClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowLayoutSettings(true);
    }

    _Use_decl_annotations_
    void EditorWindow::OnCloseLayoutSettingsClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowLayoutSettings(false);
    }

    _Use_decl_annotations_
    void EditorWindow::ShowLayoutSettings(bool show)
    {
        LayoutSettingsPanel().Visibility(show ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

        if (show)
        {
            RefreshLayoutSettings();
        }
        else
        {
            // Pages may have been added, removed or reordered while the screen was up, and the
            // tab strip and the canvas are built from that list.
            RebuildPageRail();
            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
        }
    }

    void EditorWindow::RefreshLayoutSettings()
    {
        RefreshSettingsPages();
        RefreshSettingsDevices();
    }

    // The menu item that asks for the edit lives inside the list the edit rebuilds, so the
    // rebuild waits for the click to finish rather than tearing down the handler mid-call.
    _Use_decl_annotations_
    void EditorWindow::ApplyPageEdit(std::function<bool(glass::EditorController&)> edit)
    {
        if (!edit(m_editor))
        {
            return;
        }

        MarkChanged();

        if (m_dispatcher != nullptr)
        {
            m_dispatcher.TryEnqueue([weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->RefreshSettingsPages();
                    }
                });
        }
    }

    _Use_decl_annotations_
    void EditorWindow::ApplyDeviceEdit(std::function<bool(glass::EditorController&)> edit)
    {
        if (!edit(m_editor))
        {
            return;
        }

        MarkChanged();

        if (m_dispatcher != nullptr)
        {
            m_dispatcher.TryEnqueue([weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->RefreshSettingsDevices();
                        strong->RefreshInspector();
                    }
                });
        }
    }

    // ------------------------------------------------------------------ pages

    void EditorWindow::RefreshSettingsPages()
    {
        auto list = SettingsPageList();

        list.Children().Clear();

        auto const& document = m_editor.Document();
        auto const pageCount = document.Pages.size();

        for (size_t index = 0; index < pageCount; ++index)
        {
            auto const& page = document.Pages[index];

            controls::Border card{};
            card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6.0));
            card.Background(BrushFromKey(L"SubtleFillColorSecondaryBrush"));
            card.Padding(xaml::ThicknessHelper::FromLengths(11.0, 6.0, 4.0, 6.0));

            controls::Grid row{};
            row.ColumnSpacing(8.0);

            for (auto const width : { xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star),
                                      xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto),
                                      xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto) })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                row.ColumnDefinitions().Append(column);
            }

            auto name = MakeText(
                winrt::hstring{ page.Name.empty()
                    ? std::wstring{ resources::GetString(L"UntitledPage") }
                    : page.Name },
                13.0,
                L"TextFillColorPrimaryBrush");

            if (page.IsSharedBand)
            {
                name.FontWeight(text::FontWeights::SemiBold());
            }

            controls::Grid::SetColumn(name, 0);

            auto const controlCount = page.Controls.size();

            auto count = MakeText(
                resources::FormatString(
                    controlCount == 1 ? L"OneControlFormat" : L"ControlCountFormat",
                    std::to_wstring(controlCount)),
                11.0,
                L"TextFillColorTertiaryBrush");

            controls::Grid::SetColumn(count, 1);

            auto menuButton = MakeMenuButton();
            controls::Grid::SetColumn(menuButton, 2);

            controls::MenuFlyout menu{};

            auto weak = get_weak();

            {
                controls::MenuFlyoutItem item{};
                item.Text(resources::GetString(L"PageMenuRename"));
                item.Click([weak, index](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShowRenamePageDialog(index);
                        }
                    });
                menu.Items().Append(item);
            }

            {
                controls::MenuFlyoutItem item{};
                item.Text(resources::GetString(L"PageMenuMoveUp"));
                item.IsEnabled(index > 0);
                item.Click([weak, index](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ApplyPageEdit([index](glass::EditorController& editor)
                                { return editor.MovePage(index, true); });
                        }
                    });
                menu.Items().Append(item);
            }

            {
                controls::MenuFlyoutItem item{};
                item.Text(resources::GetString(L"PageMenuMoveDown"));
                item.IsEnabled(index + 1 < pageCount);
                item.Click([weak, index](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ApplyPageEdit([index](glass::EditorController& editor)
                                { return editor.MovePage(index, false); });
                        }
                    });
                menu.Items().Append(item);
            }

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            {
                controls::ToggleMenuFlyoutItem item{};
                item.Text(resources::GetString(L"PageMenuAlwaysOnScreen"));
                item.IsChecked(page.IsSharedBand);
                item.Click([weak, index](auto&& sender, auto&&)
                    {
                        auto const wanted = sender.template as<controls::ToggleMenuFlyoutItem>().IsChecked();

                        if (auto strong = weak.get())
                        {
                            strong->ApplyPageEdit([index, wanted](glass::EditorController& editor)
                                { return editor.SetPageIsSharedBand(index, wanted); });
                        }
                    });
                menu.Items().Append(item);
            }

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            {
                controls::MenuFlyoutItem item{};
                item.Text(resources::GetString(L"PageMenuDelete"));
                item.IsEnabled(pageCount > 1);
                item.Click([weak, index](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->RemovePageWithConfirmation(index);
                        }
                    });
                menu.Items().Append(item);
            }

            menuButton.Flyout(menu);

            row.Children().Append(name);
            row.Children().Append(count);
            row.Children().Append(menuButton);

            card.Child(row);
            list.Children().Append(card);
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::RemovePageWithConfirmation(size_t index)
    {
        auto strong = get_strong();

        try
        {
            auto const& document = m_editor.Document();

            if (index >= document.Pages.size() || document.Pages.size() < 2)
            {
                co_return;
            }

            auto const controlCount = document.Pages[index].Controls.size();

            // An empty page is a click, not a decision. Asking about one is the kind of prompt
            // that teaches people to dismiss prompts.
            if (controlCount == 0)
            {
                ApplyPageEdit([index](glass::EditorController& editor)
                    { return editor.RemovePage(index); });

                RebuildPageRail();
                co_return;
            }

            if (m_openDialog != nullptr)
            {
                co_return;
            }

            auto const pageName = document.Pages[index].Name;

            // Where the controls could go instead. Built before the dialog so the indexes are
            // taken from the document as it is now.
            std::vector<size_t> destinations{};

            for (size_t other = 0; other < document.Pages.size(); ++other)
            {
                if (other != index)
                {
                    destinations.push_back(other);
                }
            }

            controls::StackPanel panel{};
            panel.Spacing(10.0);
            panel.Width(380.0);

            controls::TextBlock body{};
            body.TextWrapping(xaml::TextWrapping::Wrap);
            body.Text(resources::FormatString(
                controlCount == 1 ? L"RemovePageOneControlFormat" : L"RemovePageControlsFormat",
                pageName,
                std::to_wstring(controlCount)));

            panel.Children().Append(body);

            controls::RadioButton moveThem{};
            moveThem.Content(box_value(resources::GetString(L"RemovePageMoveThem")));
            moveThem.GroupName(L"removepage");
            moveThem.IsChecked(true);

            controls::ComboBox destinationCombo{};
            destinationCombo.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            destinationCombo.Margin(xaml::ThicknessHelper::FromLengths(28.0, 0.0, 0.0, 0.0));

            // It reads as the tail of the radio button above it, so it carries no visible
            // header. Without a name of its own a screen reader would announce nothing at all.
            automation::AutomationProperties::SetName(
                destinationCombo, resources::GetString(L"RemovePageDestinationName"));

            for (auto const other : destinations)
            {
                auto const& page = document.Pages[other];

                destinationCombo.Items().Append(box_value(winrt::hstring{
                    page.Name.empty()
                        ? std::wstring{ resources::GetString(L"UntitledPage") }
                        : page.Name }));
            }

            destinationCombo.SelectedIndex(0);

            // With only one other page there is nothing to choose, so the list is a sentence
            // rather than a control.
            destinationCombo.Visibility(destinations.size() > 1
                ? xaml::Visibility::Visible
                : xaml::Visibility::Collapsed);

            if (destinations.size() == 1)
            {
                moveThem.Content(box_value(resources::FormatString(
                    L"RemovePageMoveToFormat",
                    document.Pages[destinations[0]].Name)));
            }

            controls::RadioButton deleteThem{};
            deleteThem.Content(box_value(resources::GetString(L"RemovePageDeleteThem")));
            deleteThem.GroupName(L"removepage");

            panel.Children().Append(moveThem);
            panel.Children().Append(destinationCombo);
            panel.Children().Append(deleteThem);

            controls::ContentDialog dialog{};
            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"RemovePageTitle")));
            dialog.PrimaryButtonText(resources::GetString(L"RemovePageAccept"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Close);
            dialog.Content(panel);

            m_openDialog = dialog.ShowAsync();
            auto const result = co_await m_openDialog;
            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const move = moveThem.IsChecked().GetBoolean();

            auto const chosen = destinationCombo.SelectedIndex();

            auto const destination = (chosen >= 0 && static_cast<size_t>(chosen) < destinations.size())
                ? destinations[static_cast<size_t>(chosen)]
                : destinations.front();

            auto const changed = move
                ? m_editor.MoveControlsAndRemovePage(index, destination)
                : m_editor.RemovePage(index);

            if (changed)
            {
                RefreshSettingsPages();
                RebuildPageRail();
                BuildPage();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to remove the page.")

        m_openDialog = nullptr;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::ShowRenamePageDialog(size_t index)
    {
        auto strong = get_strong();

        try
        {
            if (m_openDialog != nullptr || index >= m_editor.Document().Pages.size())
            {
                co_return;
            }

            controls::TextBox nameBox{};
            nameBox.Header(box_value(resources::GetString(L"RenamePageNameLabel")));
            nameBox.Text(winrt::hstring{ m_editor.Document().Pages[index].Name });
            nameBox.Width(300.0);
            nameBox.SelectAll();

            controls::ContentDialog dialog{};
            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"RenamePageDialogTitle")));
            dialog.PrimaryButtonText(resources::GetString(L"DialogSave"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);
            dialog.Content(nameBox);

            m_openDialog = dialog.ShowAsync();
            auto const result = co_await m_openDialog;
            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            if (m_editor.RenamePage(index, std::wstring{ nameBox.Text() }))
            {
                RefreshSettingsPages();

                // The rail carries the page names too, and it is what somebody sees the moment
                // they go back to the editor.
                RebuildPageRail();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rename the page.")

        m_openDialog = nullptr;
    }

    _Use_decl_annotations_
    void EditorWindow::OnSettingsAddPageClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const name = resources::FormatString(
            L"NewPageNameFormat", std::to_wstring(m_editor.Document().Pages.size() + 1));

        if (m_editor.AddPage(std::wstring{ name }))
        {
            RefreshSettingsPages();
            MarkChanged();
        }
    }

    // ------------------------------------------------------------------ devices

    void EditorWindow::RefreshSettingsDevices()
    {
        auto list = SettingsDeviceList();

        list.Children().Clear();

        auto const endpoints = midiapp::EndpointCatalog::Current().Snapshot();
        auto const& document = m_editor.Document();

        int32_t missing{ 0 };

        for (auto const& device : document.Devices)
        {
            auto const resolved = midiapp::EndpointCatalog::Current().Resolve(
                device.Match, device.MatchMode, device.Match.TransportSuppliedEndpointName);

            auto const ready = resolved.has_value();

            if (!ready) { ++missing; }

            controls::Border card{};
            card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(7.0));
            card.Background(BrushFromKey(L"SubtleFillColorSecondaryBrush"));
            card.Padding(xaml::ThicknessHelper::FromLengths(13.0, 9.0, 5.0, 10.0));

            controls::StackPanel body{};
            body.Spacing(3.0);

            controls::Grid top{};
            top.ColumnSpacing(10.0);

            for (auto const width : { xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto),
                                      xaml::GridLengthHelper::FromPixels(92.0),
                                      xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star),
                                      xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto),
                                      xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto) })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                top.ColumnDefinitions().Append(column);
            }

            // The picture the customer gave the endpoint, or the transport's default, the same
            // way the Settings app shows it. A row of identical text is hard to scan.
            auto picture = MakeEndpointImage(
                ready ? resolved->ImagePath : std::wstring{},
                ready ? resolved->TransportCode : std::wstring{},
                32.0);

            controls::Grid::SetColumn(picture, 0);

            auto name = MakeText(winrt::hstring{ device.Name }, 13.0, L"AccentTextFillColorPrimaryBrush");
            name.FontWeight(text::FontWeights::SemiBold());
            controls::Grid::SetColumn(name, 1);

            auto hardware = MakeText(
                winrt::hstring{ ready
                    ? resolved->Name
                    : std::wstring{ device.Match.TransportSuppliedEndpointName } },
                13.0,
                L"TextFillColorPrimaryBrush");

            controls::Grid::SetColumn(hardware, 2);

            auto chip = MakeStatusChip(ready);
            controls::Grid::SetColumn(chip, 3);

            auto menuButton = MakeMenuButton();
            controls::Grid::SetColumn(menuButton, 4);

            auto const deviceName = device.Name;

            controls::MenuFlyout menu{};

            auto weak = get_weak();

            {
                controls::MenuFlyoutItem item{};
                item.Text(resources::GetString(L"DeviceMenuRename"));
                item.Click([weak, deviceName](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShowRenameDeviceDialog(deviceName);
                        }
                    });
                menu.Items().Append(item);
            }

            {
                controls::MenuFlyoutItem item{};
                item.Text(resources::GetString(L"DeviceMenuRepoint"));
                item.Click([weak, deviceName](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShowDevicePickerDialog(deviceName);
                        }
                    });
                menu.Items().Append(item);
            }

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            {
                controls::MenuFlyoutItem item{};
                item.Text(resources::GetString(L"DeviceMenuRemove"));
                item.Click([weak, deviceName](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->RemoveDeviceWithConfirmation(deviceName);
                        }
                    });
                menu.Items().Append(item);
            }

            menuButton.Flyout(menu);

            top.Children().Append(picture);
            top.Children().Append(name);
            top.Children().Append(hardware);
            top.Children().Append(chip);
            top.Children().Append(menuButton);

            body.Children().Append(top);

            // Second line: how it is found, and how many controls would notice if it went.
            controls::StackPanel detail{};
            detail.Orientation(controls::Orientation::Horizontal);
            detail.Spacing(9.0);
            detail.Margin(xaml::ThicknessHelper::FromLengths(0.0, 0.0, 0.0, 0.0));

            detail.Children().Append(MakeText(
                resources::FormatString(
                    L"DeviceFoundByFormat",
                    resources::GetString(MatchModeResourceKey(device.MatchMode))),
                11.0,
                L"TextFillColorTertiaryBrush"));

            auto const users = m_editor.CountControlsUsingDevice(device.Name);

            detail.Children().Append(MakeText(
                resources::FormatString(
                    users == 1 ? L"DeviceOneControlUsesFormat" : L"DeviceControlsUseFormat",
                    std::to_wstring(users)),
                11.0,
                L"TextFillColorTertiaryBrush"));

            body.Children().Append(detail);

            // The match mode radio row from the comp. Changing it is the fix for "I plugged the
            // same keyboard into a different port".
            controls::StackPanel modes{};
            modes.Orientation(controls::Orientation::Horizontal);
            modes.Spacing(4.0);
            modes.VerticalAlignment(xaml::VerticalAlignment::Center);
            modes.Margin(xaml::ThicknessHelper::FromLengths(0.0, 4.0, 0.0, 0.0));

            modes.Children().Append(MakeText(
                resources::GetString(L"DeviceFindItNextTime"), 11.0, L"TextFillColorTertiaryBrush"));

            for (size_t modeIndex = 0; modeIndex < std::size(MatchModeOrder); ++modeIndex)
            {
                auto const mode = MatchModeOrder[modeIndex];

                controls::RadioButton radio{};
                radio.Content(box_value(resources::GetString(MatchModeKeys[modeIndex])));
                radio.GroupName(winrt::hstring{ L"match:" + deviceName });
                radio.IsChecked(device.MatchMode == mode);
                radio.FontSize(11.0);
                radio.MinWidth(0.0);

                // The stock template reserves a 32 px row and top-aligns its glyph in it, which
                // leaves the circles riding above the caption beside them.
                radio.MinHeight(0.0);
                radio.VerticalAlignment(xaml::VerticalAlignment::Center);
                radio.VerticalContentAlignment(xaml::VerticalAlignment::Center);
                radio.Padding(xaml::ThicknessHelper::FromLengths(6.0, 0.0, 10.0, 0.0));

                // A USB rule on something that is not USB would never match, so it is offered
                // only where it could work.
                radio.IsEnabled(mode != midiapp::EndpointMatchMode::UsbVendorAndProduct ||
                    device.Match.HasUsbIdentity());

                radio.Checked([weak, deviceName, mode](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ApplyDeviceEdit([deviceName, mode](glass::EditorController& editor)
                                { return editor.SetDeviceMatchMode(deviceName, mode); });
                        }
                    });

                modes.Children().Append(radio);
            }

            body.Children().Append(modes);

            card.Child(body);
            list.Children().Append(card);
        }

        if (document.Devices.empty())
        {
            list.Children().Append(MakeText(
                resources::GetString(L"NoDevicesYet"), 12.0, L"TextFillColorTertiaryBrush"));
        }

        LayoutSettingsWarningText().Text(missing == 0
            ? winrt::hstring{}
            : resources::FormatString(
                missing == 1 ? L"OneDeviceMissingFormat" : L"DevicesMissingFormat",
                std::to_wstring(missing)));

        SettingsAddDeviceButton().IsEnabled(!endpoints.empty());
    }

    _Use_decl_annotations_
    void EditorWindow::OnSettingsAddDeviceClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowDevicePickerDialog({});
    }

    _Use_decl_annotations_
    void EditorWindow::OnSettingsLookAgainClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        // The watcher starts on a background thread, so a designer opened straight after launch
        // can reach this screen before there is anything to show.
        midiapp::EndpointCatalog::Current().Refresh();

        RefreshSettingsDevices();
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::ShowDevicePickerDialog(std::wstring existingName)
    {
        auto strong = get_strong();

        try
        {
            if (m_openDialog != nullptr)
            {
                co_return;
            }

            auto const endpoints = midiapp::EndpointCatalog::Current().Snapshot();

            controls::StackPanel panel{};
            panel.Spacing(10.0);
            panel.Width(DevicePickerWidth);

            controls::TextBox nameBox{};
            nameBox.Header(box_value(resources::GetString(L"DevicePickerNameLabel")));

            // Renaming is a separate command, so an existing entry keeps its name here and the
            // box is only there to say what the new one will be called.
            nameBox.Text(winrt::hstring{ existingName });
            nameBox.IsEnabled(existingName.empty());

            controls::ListView endpointList{};
            endpointList.Header(box_value(resources::GetString(L"DevicePickerEndpointLabel")));
            endpointList.MaxHeight(340.0);
            endpointList.SelectionMode(controls::ListViewSelectionMode::Single);

            // A ListView sizes its item containers to their content unless it is told not to,
            // so without this a long description makes the row wider than the list and gets
            // clipped by the dialog instead of trimmed with an ellipsis. Both halves are
            // needed: the container has to stretch, and the list has to stop offering its
            // rows infinite width to measure against.
            controls::ScrollViewer::SetHorizontalScrollMode(endpointList, controls::ScrollMode::Disabled);
            controls::ScrollViewer::SetHorizontalScrollBarVisibility(
                endpointList, controls::ScrollBarVisibility::Disabled);

            xaml::Style itemStyle{ winrt::xaml_typename<controls::ListViewItem>() };

            itemStyle.Setters().Append(xaml::Setter{
                controls::Control::HorizontalContentAlignmentProperty(),
                box_value(xaml::HorizontalAlignment::Stretch) });

            itemStyle.Setters().Append(xaml::Setter{
                controls::Control::VerticalContentAlignmentProperty(),
                box_value(xaml::VerticalAlignment::Center) });

            // Each endpoint gets a card of its own. The rows carry one, two or three lines of
            // text depending on what the device declares, so without a floor they come out
            // different heights and the list reads as ragged rather than as a list.
            itemStyle.Setters().Append(xaml::Setter{
                xaml::FrameworkElement::MinHeightProperty(), box_value(DevicePickerRowHeight) });

            itemStyle.Setters().Append(xaml::Setter{
                controls::Control::PaddingProperty(),
                box_value(xaml::ThicknessHelper::FromLengths(12.0, 9.0, 12.0, 9.0)) });

            itemStyle.Setters().Append(xaml::Setter{
                xaml::FrameworkElement::MarginProperty(),
                box_value(xaml::ThicknessHelper::FromLengths(0.0, 0.0, 0.0, 6.0)) });

            itemStyle.Setters().Append(xaml::Setter{
                controls::Control::CornerRadiusProperty(),
                box_value(xaml::CornerRadiusHelper::FromUniformRadius(6.0)) });

            // The rest color. Hover and selection come from the container's own states, which
            // is what puts the system highlight color on them rather than one invented here.
            itemStyle.Setters().Append(xaml::Setter{
                controls::Control::BackgroundProperty(),
                xaml::Application::Current().Resources().Lookup(
                    box_value(L"CardBackgroundFillColorDefaultBrush")) });

            endpointList.ItemContainerStyle(itemStyle);

            // Room for the rounded corners to sit in rather than against the list's own edge.
            endpointList.Padding(xaml::ThicknessHelper::FromLengths(2.0, 2.0, 2.0, 0.0));

            for (auto const& endpoint : endpoints)
            {
                controls::Grid row{};
                row.ColumnSpacing(12.0);
                row.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                row.VerticalAlignment(xaml::VerticalAlignment::Center);

                for (auto const width : { xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto),
                                          xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star) })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    row.ColumnDefinitions().Append(column);
                }

                auto picture = MakeEndpointImage(endpoint.ImagePath, endpoint.TransportCode, 30.0);
                controls::Grid::SetColumn(picture, 0);

                controls::StackPanel text{};
                text.Spacing(1.0);
                text.VerticalAlignment(xaml::VerticalAlignment::Center);
                controls::Grid::SetColumn(text, 1);

                text.Children().Append(MakeRowLine(
                    winrt::hstring{ endpoint.Name }, 13.0, L"TextFillColorPrimaryBrush"));

                if (!endpoint.Description.empty())
                {
                    text.Children().Append(MakeRowLine(
                        winrt::hstring{ endpoint.Description }, 11.0, L"TextFillColorTertiaryBrush"));
                }

                if (auto const groups = DescribeEndpointGroups(endpoint); !groups.empty())
                {
                    text.Children().Append(MakeRowLine(
                        winrt::hstring{ groups }, 11.0, L"TextFillColorTertiaryBrush"));
                }

                row.Children().Append(picture);
                row.Children().Append(text);

                endpointList.Items().Append(row);
            }

            if (!endpoints.empty())
            {
                endpointList.SelectedIndex(0);
            }

            // Picking an endpoint fills a name in, because a table entry called "Synth" that a
            // customer had to invent from nothing is the step everybody skips.
            endpointList.SelectionChanged([nameBox, endpoints, existingName](auto&& sender, auto&&)
                {
                    if (!existingName.empty() || !nameBox.Text().empty())
                    {
                        return;
                    }

                    auto const index = sender.template as<controls::ListView>().SelectedIndex();

                    if (index >= 0 && static_cast<size_t>(index) < endpoints.size())
                    {
                        nameBox.Text(winrt::hstring{ endpoints[static_cast<size_t>(index)].Name });
                    }
                });

            controls::ComboBox modeCombo{};
            modeCombo.Header(box_value(resources::GetString(L"DevicePickerMatchLabel")));
            modeCombo.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const* const key : MatchModeKeys)
            {
                modeCombo.Items().Append(box_value(resources::GetString(key)));
            }

            modeCombo.SelectedIndex(0);

            panel.Children().Append(nameBox);
            panel.Children().Append(endpointList);
            panel.Children().Append(modeCombo);

            if (endpoints.empty())
            {
                panel.Children().Append(MakeText(
                    resources::GetString(L"DevicePickerNothingHere"), 12.0, L"TextFillColorTertiaryBrush"));
            }

            controls::ContentDialog dialog{};
            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(
                existingName.empty() ? L"DevicePickerAddTitle" : L"DevicePickerRepointTitle")));
            dialog.PrimaryButtonText(resources::GetString(
                existingName.empty() ? L"DialogAdd" : L"DialogSave"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);
            dialog.IsPrimaryButtonEnabled(!endpoints.empty());
            dialog.Content(panel);

            m_openDialog = dialog.ShowAsync();
            auto const result = co_await m_openDialog;
            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const chosen = endpointList.SelectedIndex();

            if (chosen < 0 || static_cast<size_t>(chosen) >= endpoints.size())
            {
                co_return;
            }

            auto const& endpoint = endpoints[static_cast<size_t>(chosen)];

            auto const modeIndex = modeCombo.SelectedIndex();

            auto const mode = (modeIndex >= 0 && static_cast<size_t>(modeIndex) < std::size(MatchModeOrder))
                ? MatchModeOrder[static_cast<size_t>(modeIndex)]
                : midiapp::EndpointMatchMode::EndpointDeviceId;

            auto changed = false;

            if (existingName.empty())
            {
                glass::DeviceEntry entry{};

                entry.Name = std::wstring{ nameBox.Text() };
                entry.Match = endpoint.BuildMatch();
                entry.MatchMode = mode;

                if (entry.Name.empty())
                {
                    entry.Name = endpoint.Name;
                }

                changed = m_editor.AddDevice(entry);
            }
            else
            {
                changed = m_editor.SetDeviceMatch(existingName, endpoint.BuildMatch(), mode);
            }

            if (changed)
            {
                RefreshSettingsDevices();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the device table.")

        m_openDialog = nullptr;
    }

    _Use_decl_annotations_
    winrt::hstring EditorWindow::DescribeResolvedDevice(std::wstring const& deviceName) const
    {
        if (deviceName.empty())
        {
            return {};
        }

        auto const* const device = m_editor.Document().FindDevice(deviceName);

        if (device == nullptr)
        {
            return resources::GetString(L"DeviceNotInTable");
        }

        auto const resolved = midiapp::EndpointCatalog::Current().Resolve(
            device->Match, device->MatchMode, device->Match.TransportSuppliedEndpointName);

        if (!resolved.has_value())
        {
            return resources::GetString(L"DeviceNotHereLong");
        }

        return resources::FormatString(L"DeviceResolvedFormat", resolved->Name);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::RemoveDeviceWithConfirmation(std::wstring deviceName)
    {
        auto strong = get_strong();

        try
        {
            auto const users = m_editor.CountControlsUsingDevice(deviceName);

            // Nothing points at it, so there is nothing to warn about. A dialog that always
            // appears is one nobody reads by the third time.
            if (users > 0)
            {
                if (m_openDialog != nullptr)
                {
                    co_return;
                }

                controls::TextBlock body{};
                body.TextWrapping(xaml::TextWrapping::Wrap);
                body.Width(360.0);
                body.Text(resources::FormatString(
                    users == 1 ? L"RemoveDeviceOneInUseFormat" : L"RemoveDeviceInUseFormat",
                    deviceName,
                    std::to_wstring(users)));

                controls::ContentDialog dialog{};
                dialog.XamlRoot(RootGrid().XamlRoot());
                dialog.Title(box_value(resources::GetString(L"RemoveDeviceTitle")));
                dialog.PrimaryButtonText(resources::GetString(L"RemoveDeviceAccept"));
                dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
                dialog.DefaultButton(controls::ContentDialogButton::Close);
                dialog.Content(body);

                m_openDialog = dialog.ShowAsync();
                auto const result = co_await m_openDialog;
                m_openDialog = nullptr;

                if (result != controls::ContentDialogResult::Primary)
                {
                    co_return;
                }
            }

            if (m_editor.RemoveDevice(deviceName))
            {
                RefreshSettingsDevices();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to remove the device.")

        m_openDialog = nullptr;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::ShowRenameDeviceDialog(std::wstring deviceName)
    {
        auto strong = get_strong();

        try
        {
            if (m_openDialog != nullptr)
            {
                co_return;
            }

            controls::TextBox nameBox{};
            nameBox.Header(box_value(resources::GetString(L"DevicePickerNameLabel")));
            nameBox.Text(winrt::hstring{ deviceName });
            nameBox.Width(300.0);
            nameBox.SelectAll();

            controls::StackPanel panel{};
            panel.Spacing(9.0);

            panel.Children().Append(nameBox);

            panel.Children().Append(MakeText(
                resources::GetString(L"DeviceRenameNote"), 11.0, L"TextFillColorTertiaryBrush"));

            controls::ContentDialog dialog{};
            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"DeviceRenameTitle")));
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

            if (m_editor.RenameDevice(deviceName, std::wstring{ nameBox.Text() }))
            {
                RefreshSettingsDevices();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rename the device.")

        m_openDialog = nullptr;
    }
}
