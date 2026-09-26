// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The editor's three dialogs: Repeat, the page size, and renaming the layout.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "PageTemplates.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

        media::Brush BrushNamed(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources().Lookup(box_value(key)).as<media::Brush>();
        }

        controls::ContentDialog MakeDialog(
            _In_ xaml::XamlRoot const& root,
            _In_ winrt::hstring const& title,
            _In_ winrt::hstring const& primary)
        {
            controls::ContentDialog dialog{};

            dialog.XamlRoot(root);
            dialog.Title(box_value(title));
            dialog.PrimaryButtonText(primary);
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            return dialog;
        }

        controls::NumberBox MakeNumberField(
            _In_ winrt::hstring const& header,
            _In_ double value,
            _In_ double minimum,
            _In_ double maximum)
        {
            controls::NumberBox box{};

            box.Header(box_value(header));
            box.Value(value);
            box.Minimum(minimum);
            box.Maximum(maximum);
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);

            return box;
        }

        controls::ComboBox MakeChoiceField(
            _In_ winrt::hstring const& header,
            _In_ std::vector<winrt::hstring> const& options,
            _In_ int32_t selected)
        {
            controls::ComboBox combo{};

            combo.Header(box_value(header));
            combo.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const& option : options)
            {
                combo.Items().Append(box_value(option));
            }

            combo.SelectedIndex(selected);

            return combo;
        }

        // Two labeled things side by side, which is how the design lays out the anchor and its
        // preview.
        controls::Grid MakeTwoUpRow(
            _In_ winrt::hstring const& leftLabel,
            _In_ winrt::hstring const& rightLabel,
            _In_ xaml::UIElement const& left,
            _In_ xaml::UIElement const& right)
        {
            controls::Grid row{};

            row.ColumnSpacing(16);

            for (int32_t column = 0; column < 2; ++column)
            {
                controls::ColumnDefinition definition{};
                definition.Width(xaml::GridLengthHelper::FromValueAndType(0, xaml::GridUnitType::Auto));
                row.ColumnDefinitions().Append(definition);
            }

            auto const stack = [](winrt::hstring const& label, xaml::UIElement const& content)
                {
                    controls::StackPanel panel{};

                    panel.Spacing(6);

                    controls::TextBlock text{};

                    text.Text(label);
                    text.FontSize(11);
                    text.Foreground(BrushNamed(L"TextFillColorTertiaryBrush"));

                    panel.Children().Append(text);
                    panel.Children().Append(content);

                    return panel;
                };

            auto leftPanel = stack(leftLabel, left);
            auto rightPanel = stack(rightLabel, right);

            controls::Grid::SetColumn(leftPanel, 0);
            controls::Grid::SetColumn(rightPanel, 1);

            row.Children().Append(leftPanel);
            row.Children().Append(rightPanel);

            return row;
        }

        // One of the two shrink choices: a title, and underneath it what that choice costs.
        // Writing the consequence out is the whole point - "11 controls would end up outside the
        // page" is the sentence that stops somebody picking the wrong one.
        struct ChoiceCard
        {
            controls::Primitives::ToggleButton Toggle{ nullptr };
            controls::TextBlock Body{ nullptr };
            xaml::UIElement Root{ nullptr };
        };

        ChoiceCard MakeChoiceCard(_In_ winrt::hstring const& title, _In_ winrt::hstring const& body)
        {
            ChoiceCard card{};

            controls::StackPanel panel{};

            panel.Spacing(4);

            controls::TextBlock titleText{};

            titleText.Text(title);
            titleText.FontSize(13);
            titleText.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            titleText.TextWrapping(xaml::TextWrapping::Wrap);

            card.Body = controls::TextBlock{};
            card.Body.Text(body);
            card.Body.FontSize(11.5);
            card.Body.TextWrapping(xaml::TextWrapping::Wrap);
            card.Body.Foreground(BrushNamed(L"TextFillColorSecondaryBrush"));

            panel.Children().Append(titleText);
            panel.Children().Append(card.Body);

            card.Toggle = controls::Primitives::ToggleButton{};

            card.Toggle.Content(panel);
            card.Toggle.Padding({ 13, 11, 13, 11 });
            card.Toggle.CornerRadius({ 6, 6, 6, 6 });
            card.Toggle.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            card.Toggle.HorizontalContentAlignment(xaml::HorizontalAlignment::Left);
            card.Toggle.Style(xaml::Application::Current().Resources()
                .Lookup(box_value(L"ChoiceCardStyle")).as<xaml::Style>());

            xaml::Automation::AutomationProperties::SetName(card.Toggle, title);

            card.Root = card.Toggle;

            return card;
        }

        // The nine-position anchor. Where the controls already on the page sit inside a page
        // that just got bigger.
        struct AnchorPicker : winrt::implements<AnchorPicker, winrt::Windows::Foundation::IInspectable>
        {
            AnchorPicker()
            {
                m_root.ColumnSpacing(3);
                m_root.RowSpacing(3);

                for (int32_t i = 0; i < 3; ++i)
                {
                    controls::ColumnDefinition column{};
                    column.Width(xaml::GridLengthHelper::FromValueAndType(0, xaml::GridUnitType::Auto));
                    m_root.ColumnDefinitions().Append(column);

                    controls::RowDefinition row{};
                    row.Height(xaml::GridLengthHelper::FromValueAndType(0, xaml::GridUnitType::Auto));
                    m_root.RowDefinitions().Append(row);
                }

                auto const style = xaml::Application::Current().Resources()
                    .Lookup(box_value(L"AnchorCellStyle")).as<xaml::Style>();

                for (int32_t index = 0; index < 9; ++index)
                {
                    controls::Primitives::ToggleButton cell{};

                    cell.Style(style);
                    cell.Tag(box_value(winrt::hstring{ std::to_wstring(index) }));
                    cell.IsChecked(index == 0);

                    xaml::Automation::AutomationProperties::SetName(
                        cell, resources::GetString(AnchorNames[index]));

                    controls::ToolTipService::SetToolTip(
                        cell, box_value(resources::GetString(AnchorNames[index])));

                    cell.Checked([this](auto&& sender, auto&&)
                        {
                            auto const button = sender.template as<controls::Primitives::ToggleButton>();

                            m_anchor = static_cast<glass::CanvasAnchor>(std::stoi(std::wstring{
                                winrt::unbox_value_or<winrt::hstring>(button.Tag(), L"0") }));

                            for (auto const& other : m_cells)
                            {
                                if (other != button)
                                {
                                    other.IsChecked(false);
                                }
                            }

                            if (m_changed)
                            {
                                m_changed();
                            }
                        });

                    // There is always an anchor, so unpicking the one that is on puts it back.
                    cell.Unchecked([this](auto&& sender, auto&&)
                        {
                            auto const button = sender.template as<controls::Primitives::ToggleButton>();

                            auto const index = std::stoi(std::wstring{
                                winrt::unbox_value_or<winrt::hstring>(button.Tag(), L"0") });

                            if (static_cast<glass::CanvasAnchor>(index) == m_anchor)
                            {
                                button.IsChecked(true);
                            }
                        });

                    controls::Grid::SetColumn(cell, index % 3);
                    controls::Grid::SetRow(cell, index / 3);

                    m_cells.push_back(cell);
                    m_root.Children().Append(cell);
                }
            }

            controls::Grid Root() const noexcept { return m_root; }
            glass::CanvasAnchor Anchor() const noexcept { return m_anchor; }

            void Changed(_In_ std::function<void()> handler) { m_changed = std::move(handler); }

        private:
            static constexpr wchar_t const* AnchorNames[]
            {
                L"AnchorTopLeft", L"AnchorTop", L"AnchorTopRight",
                L"AnchorLeft", L"AnchorCenter", L"AnchorRight",
                L"AnchorBottomLeft", L"AnchorBottom", L"AnchorBottomRight",
            };

            controls::Grid m_root{};
            std::vector<controls::Primitives::ToggleButton> m_cells{};
            glass::CanvasAnchor m_anchor{ glass::CanvasAnchor::TopLeft };
            std::function<void()> m_changed{};
        };
    }

    // ---------------------------------------------------------------- repeat

    // The single biggest time saver in the app. Building a sixteen channel mixer by hand means
    // sixteen copies and sixteen hand-edited channel numbers, and one of them will be wrong.
    winrt::fire_and_forget EditorWindow::ShowRepeatDialog()
    {
        auto strong = get_strong();

        try
        {
            if (m_editor.Selection().empty() || m_openDialog != nullptr)
            {
                co_return;
            }

            auto copies = MakeNumberField(
                resources::GetString(L"RepeatCopiesLabel"), 7, 1, glass::MaximumRepeatCopies);

            auto direction = MakeChoiceField(
                resources::GetString(L"RepeatDirectionLabel"),
                {
                    resources::GetString(L"RepeatDirectionRight"),
                    resources::GetString(L"RepeatDirectionLeft"),
                    resources::GetString(L"RepeatDirectionDown"),
                    resources::GetString(L"RepeatDirectionUp"),
                },
                0);

            auto gap = MakeNumberField(resources::GetString(L"RepeatGapLabel"), 16, -512, 512);

            auto field = MakeChoiceField(
                resources::GetString(L"RepeatFieldLabel"),
                {
                    resources::GetString(L"RepeatFieldNothing"),
                    resources::GetString(L"RepeatFieldChannel"),
                    resources::GetString(L"RepeatFieldController"),
                    resources::GetString(L"RepeatFieldNote"),
                    resources::GetString(L"RepeatFieldGroup"),
                },
                2);

            auto step = MakeNumberField(resources::GetString(L"RepeatStepLabel"), 1, -64, 64);

            controls::TextBox label{};
            label.Header(box_value(resources::GetString(L"RepeatLabelLabel")));
            label.PlaceholderText(resources::GetString(L"RepeatLabelPlaceholder"));

            controls::TextBlock explanation{};
            explanation.Text(resources::GetString(L"RepeatExplanation"));
            explanation.FontSize(11);
            explanation.TextWrapping(xaml::TextWrapping::Wrap);
            explanation.Foreground(xaml::Application::Current().Resources()
                .Lookup(box_value(L"TextFillColorTertiaryBrush")).as<media::Brush>());

            controls::StackPanel panel{};
            panel.Spacing(10);
            panel.Width(320);

            panel.Children().Append(copies);
            panel.Children().Append(direction);
            panel.Children().Append(gap);
            panel.Children().Append(field);
            panel.Children().Append(step);
            panel.Children().Append(label);
            panel.Children().Append(explanation);

            auto dialog = MakeDialog(
                RootGrid().XamlRoot(),
                resources::GetString(L"RepeatDialogTitle"),
                resources::GetString(L"RepeatDialogAccept"));

            dialog.Content(panel);

            m_openDialog = dialog.ShowAsync();
            auto const result = co_await m_openDialog;
            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            glass::RepeatOptions options{};

            options.Copies = static_cast<int32_t>(std::lround(copies.Value()));
            options.Direction = static_cast<glass::RepeatDirection>(std::max(0, direction.SelectedIndex()));
            options.Gap = gap.Value();
            options.Field = static_cast<glass::RepeatField>(std::max(0, field.SelectedIndex()));
            options.Step = static_cast<int32_t>(std::lround(step.Value()));
            options.LabelPattern = std::wstring{ label.Text() };

            if (m_editor.RepeatSelection(options))
            {
                RebuildSurface();
                RebuildOutline();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to repeat the selection.")

        m_openDialog = nullptr;
    }

    // ---------------------------------------------------------------- page size

    // Growing asks where the existing controls should sit; shrinking offers to scale them. In
    // neither case is a control ever clamped inside the page, because that is what would make a
    // resize unrecoverable.
    //
    // The two cases look different on purpose, the way the design draws them: growing shows a
    // nine-position anchor and a preview of where the old page lands inside the new one, and
    // shrinking shows two choices with the consequence of each written out.
    winrt::fire_and_forget EditorWindow::ShowPageSizeDialog()
    {
        auto strong = get_strong();

        try
        {
            if (m_openDialog != nullptr)
            {
                co_return;
            }

            auto const& document = m_editor.Document();

            auto const startWidth = document.PageWidth;
            auto const startHeight = document.PageHeight;

            std::vector<winrt::hstring> presetNames{};

            for (auto const& preset : glass::PageTemplates())
            {
                presetNames.push_back(resources::FormatString(
                    L"PagePresetFormat",
                    resources::GetString(preset.ResourceKey),
                    std::to_wstring(preset.Width),
                    std::to_wstring(preset.Height)));
            }

            presetNames.push_back(resources::GetString(L"PagePresetCustom"));

            auto preset = MakeChoiceField(
                resources::GetString(L"PageSizePresetLabel"),
                presetNames,
                static_cast<int32_t>(presetNames.size()) - 1);

            auto width = MakeNumberField(
                resources::GetString(L"PageSizeWidthLabel"), startWidth, 320, 8192);

            auto height = MakeNumberField(
                resources::GetString(L"PageSizeHeightLabel"), startHeight, 240, 8192);

            // "was 1280 x 800", beside the fields, because the number being changed away from
            // is the one nobody can see any more.
            controls::TextBlock wasText{};

            wasText.FontSize(11);
            wasText.VerticalAlignment(xaml::VerticalAlignment::Bottom);
            wasText.Margin({ 0, 0, 0, 6 });
            wasText.Text(resources::FormatString(
                L"PageSizeWasFormat", std::to_wstring(startWidth), std::to_wstring(startHeight)));
            wasText.Foreground(xaml::Application::Current().Resources()
                .Lookup(box_value(L"TextFillColorTertiaryBrush")).as<media::Brush>());

            // ---- growing: where the existing controls sit, and what that looks like ----

            auto const anchorHost = winrt::make_self<AnchorPicker>();

            controls::Grid previewHost{};

            previewHost.Width(132);
            previewHost.Height(86);

            shapes::Rectangle previewNew{};

            previewNew.RadiusX(3);
            previewNew.RadiusY(3);
            previewNew.StrokeThickness(1);
            previewNew.UseLayoutRounding(false);
            previewNew.Stroke(xaml::Application::Current().Resources()
                .Lookup(box_value(L"AccentFillColorDefaultBrush")).as<media::Brush>());
            previewNew.Fill(media::SolidColorBrush(
                winrt::Windows::UI::ColorHelper::FromArgb(30, 96, 205, 255)));

            shapes::Rectangle previewOld{};

            previewOld.RadiusX(2);
            previewOld.RadiusY(2);
            previewOld.StrokeThickness(1);
            previewOld.UseLayoutRounding(false);
            previewOld.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            previewOld.VerticalAlignment(xaml::VerticalAlignment::Top);
            previewOld.Stroke(xaml::Application::Current().Resources()
                .Lookup(box_value(L"TextFillColorSecondaryBrush")).as<media::Brush>());

            {
                media::DoubleCollection dashes{};
                dashes.Append(3.0);
                dashes.Append(3.0);
                previewOld.StrokeDashArray(dashes);
            }

            previewHost.Children().Append(previewNew);
            previewHost.Children().Append(previewOld);

            auto growPanel = MakeTwoUpRow(
                resources::GetString(L"PageSizeAnchorLabel"),
                resources::GetString(L"PageSizePreviewLabel"),
                anchorHost->Root(),
                previewHost);

            // ---- shrinking: two choices, each with what it costs written out ----

            auto scaleCard = MakeChoiceCard(
                resources::GetString(L"PageSizeScaleTitle"),
                resources::GetString(L"PageSizeScaleBody"));

            auto leaveCard = MakeChoiceCard(
                resources::GetString(L"PageSizeLeaveTitle"), L"");

            scaleCard.Toggle.IsChecked(true);

            controls::StackPanel shrinkPanel{};

            shrinkPanel.Spacing(8);
            shrinkPanel.Children().Append(scaleCard.Root);
            shrinkPanel.Children().Append(leaveCard.Root);

            auto const isShrinking = [width, height, startWidth, startHeight]()
                {
                    return std::lround(width.Value()) < startWidth ||
                        std::lround(height.Value()) < startHeight;
                };

            // The number quoted is worked out before anything is committed, so nobody has to
            // press Resize to find out what it would do.
            auto const describe =
                [this, width, height, anchorHost, scaleCard, leaveCard, growPanel, shrinkPanel,
                 previewHost, previewOld, previewNew, startWidth, startHeight, isShrinking]()
                {
                    auto const newWidth = static_cast<int32_t>(std::lround(width.Value()));
                    auto const newHeight = static_cast<int32_t>(std::lround(height.Value()));

                    auto const shrinking = isShrinking();

                    growPanel.Visibility(shrinking ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
                    shrinkPanel.Visibility(shrinking ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                    glass::PageResizeRequest request{};

                    request.NewWidth = newWidth;
                    request.NewHeight = newHeight;
                    request.Anchor = anchorHost->Anchor();
                    request.ScaleContents = shrinking && scaleCard.Toggle.IsChecked().Value();

                    if (shrinking)
                    {
                        glass::PageResizeRequest leaving{ request };
                        leaving.ScaleContents = false;

                        auto const outside = m_editor.CountOutsideAfterResize(leaving);

                        leaveCard.Body.Text(outside == 0
                            ? resources::GetString(L"PageSizeLeaveBodyNoneOutside")
                            : resources::FormatString(
                                L"PageSizeLeaveBodyFormat", std::to_wstring(outside)));

                        return;
                    }

                    // The old page drawn where it would land inside the new one.
                    auto const scale = std::min(
                        previewHost.Width() / std::max(1, newWidth),
                        previewHost.Height() / std::max(1, newHeight));

                    auto const oldWidth = startWidth * scale;
                    auto const oldHeight = startHeight * scale;

                    previewNew.Width(newWidth * scale);
                    previewNew.Height(newHeight * scale);
                    previewNew.HorizontalAlignment(xaml::HorizontalAlignment::Left);
                    previewNew.VerticalAlignment(xaml::VerticalAlignment::Top);

                    auto const offset = glass::ComputePageResize(
                        startWidth, startHeight, newWidth, newHeight, anchorHost->Anchor(), false);

                    previewOld.Width(oldWidth);
                    previewOld.Height(oldHeight);
                    previewOld.Margin({ offset.OffsetX * scale, offset.OffsetY * scale, 0, 0 });
                };

            width.ValueChanged([describe](auto&&, auto&&) { describe(); });
            height.ValueChanged([describe](auto&&, auto&&) { describe(); });

            anchorHost->Changed([describe]() { describe(); });

            scaleCard.Toggle.Checked([describe, leaveCard](auto&&, auto&&)
                {
                    leaveCard.Toggle.IsChecked(false);
                    describe();
                });

            leaveCard.Toggle.Checked([describe, scaleCard](auto&&, auto&&)
                {
                    scaleCard.Toggle.IsChecked(false);
                    describe();
                });

            preset.SelectionChanged([preset, width, height](auto&&, auto&&)
                {
                    auto const index = preset.SelectedIndex();
                    auto const& templates = glass::PageTemplates();

                    if (index >= 0 && index < static_cast<int32_t>(templates.size()))
                    {
                        width.Value(templates[static_cast<size_t>(index)].Width);
                        height.Value(templates[static_cast<size_t>(index)].Height);
                    }
                });

            describe();

            controls::Grid sizeRow{};

            sizeRow.ColumnSpacing(8);

            for (int32_t column = 0; column < 3; ++column)
            {
                controls::ColumnDefinition definition{};
                definition.Width(column == 2
                    ? xaml::GridLengthHelper::FromValueAndType(0, xaml::GridUnitType::Auto)
                    : xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                sizeRow.ColumnDefinitions().Append(definition);
            }

            controls::Grid::SetColumn(width, 0);
            controls::Grid::SetColumn(height, 1);
            controls::Grid::SetColumn(wasText, 2);

            sizeRow.Children().Append(width);
            sizeRow.Children().Append(height);
            sizeRow.Children().Append(wasText);

            controls::StackPanel panel{};

            panel.Spacing(12);
            panel.Width(380);

            panel.Children().Append(preset);
            panel.Children().Append(sizeRow);
            panel.Children().Append(growPanel);
            panel.Children().Append(shrinkPanel);

            auto dialog = MakeDialog(
                RootGrid().XamlRoot(),
                resources::GetString(L"PageSizeDialogTitle"),
                resources::GetString(L"PageSizeDialogAccept"));

            dialog.Content(panel);

            m_openDialog = dialog.ShowAsync();
            auto const result = co_await m_openDialog;
            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            glass::PageResizeRequest request{};

            request.NewWidth = static_cast<int32_t>(std::lround(width.Value()));
            request.NewHeight = static_cast<int32_t>(std::lround(height.Value()));
            request.Anchor = anchorHost->Anchor();
            request.ScaleContents = isShrinking() && scaleCard.Toggle.IsChecked().Value();

            if (m_editor.ResizePage(request))
            {
                BuildPage();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the page size.")

        m_openDialog = nullptr;
    }

    // ---------------------------------------------------------------- rename

    winrt::fire_and_forget EditorWindow::ShowRenameDialog()
    {
        auto strong = get_strong();

        try
        {
            if (m_openDialog != nullptr)
            {
                co_return;
            }

            controls::TextBox nameBox{};
            nameBox.Header(box_value(resources::GetString(L"RenameLayoutNameLabel")));
            nameBox.Text(winrt::hstring{ m_editor.Document().Name });
            nameBox.SelectAll();

            controls::TextBox descriptionBox{};
            descriptionBox.Header(box_value(resources::GetString(L"RenameLayoutDescriptionLabel")));
            descriptionBox.Text(winrt::hstring{ m_editor.Document().Description });
            descriptionBox.AcceptsReturn(true);
            descriptionBox.TextWrapping(xaml::TextWrapping::Wrap);
            descriptionBox.Height(96);

            controls::StackPanel panel{};
            panel.Spacing(10);
            panel.Width(340);

            panel.Children().Append(nameBox);
            panel.Children().Append(descriptionBox);

            auto dialog = MakeDialog(
                RootGrid().XamlRoot(),
                resources::GetString(L"RenameLayoutDialogTitle"),
                resources::GetString(L"DialogSave"));

            dialog.Content(panel);

            m_openDialog = dialog.ShowAsync();
            auto const result = co_await m_openDialog;
            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto changed = m_editor.SetLayoutName(std::wstring{ nameBox.Text() });

            changed = m_editor.SetLayoutDescription(std::wstring{ descriptionBox.Text() }) || changed;

            if (changed)
            {
                // The file keeps the name it was created with. Renaming the file as well would
                // break a shortcut, a snippet reference or anything else pointing at it, and the
                // library shows the name from inside the file anyway.
                auto const title = resources::FormatString(
                    L"EditorWindowTitleFormat", m_editor.Document().Name);

                Title(title);
                AppTitleTextBlock().Text(title);

                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to rename the layout.")

        m_openDialog = nullptr;
    }
}
