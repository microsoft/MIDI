// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The label font dialog.
//
// Everything in it starts at "the theme decides" and stays there unless somebody moves it, so a
// layout nobody has styled still follows its theme when the theme changes. That is why the font
// list and the size box both carry a "same as the theme" entry rather than opening on whatever
// the theme happens to be today.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "ThemeStore.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        // In-box only. A layout is carried between machines, and a font that is not on the other
        // one is a label that silently changes shape.
        constexpr wchar_t const* LabelFonts[]
        {
            L"Segoe UI Variable Text",
            L"Segoe UI Variable Display",
            L"Segoe UI",
            L"Bahnschrift",
            L"Cascadia Mono",
            L"Consolas",
            L"Segoe UI Emoji",
        };

        // The design's own scale. A free number box invites 13.5 px labels that line up with
        // nothing else on the page.
        constexpr double LabelSizes[]{ 9, 11, 12, 14, 18, 24, 32, 48, 64 };

        struct WeightChoice
        {
            int32_t Weight{ 0 };
            wchar_t const* NameKey{ nullptr };
        };

        constexpr WeightChoice LabelWeights[]
        {
            { 0, L"FontWeightTheme" },
            { 300, L"FontWeightLight" },
            { 400, L"FontWeightNormal" },
            { 600, L"FontWeightSemiBold" },
            { 700, L"FontWeightBold" },
        };
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::ShowLabelFontDialog(std::wstring controlId)
    {
        auto lifetime = get_strong();

        try
        {
            auto const* const control = m_editor.Document().FindControl(controlId);

            if (control == nullptr)
            {
                co_return;
            }

            // Edited on a copy, so Cancel leaves nothing behind.
            auto working = std::make_shared<glass::LabelStyle>(control->LabelLook);

            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"FontDialogTitle")));
            dialog.PrimaryButtonText(resources::GetString(L"DialogDone"));
            dialog.SecondaryButtonText(resources::GetString(L"FontDialogReset"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            controls::StackPanel root{};
            root.Spacing(10);
            root.MinWidth(380);

            // ---- family ----

            controls::ComboBox family{};
            family.Header(box_value(resources::GetString(L"FontFamilyHeader")));
            family.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            family.Items().Append(box_value(resources::GetString(L"FontFamilyTheme")));

            for (auto const* const name : LabelFonts)
            {
                family.Items().Append(box_value(winrt::hstring{ name }));
            }

            family.SelectedIndex(0);

            for (int32_t index = 0; index < static_cast<int32_t>(std::size(LabelFonts)); ++index)
            {
                if (working->FontFamily == LabelFonts[index])
                {
                    family.SelectedIndex(index + 1);
                    break;
                }
            }

            // ---- size ----

            controls::ComboBox size{};
            size.Header(box_value(resources::GetString(L"FontSizeHeader")));
            size.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            size.Items().Append(box_value(resources::GetString(L"FontSizeTheme")));

            for (auto const value : LabelSizes)
            {
                size.Items().Append(box_value(winrt::to_hstring(static_cast<int32_t>(value))));
            }

            size.SelectedIndex(0);

            for (int32_t index = 0; index < static_cast<int32_t>(std::size(LabelSizes)); ++index)
            {
                if (working->FontSize == LabelSizes[index])
                {
                    size.SelectedIndex(index + 1);
                    break;
                }
            }

            // ---- weight ----

            controls::ComboBox weight{};
            weight.Header(box_value(resources::GetString(L"FontWeightHeader")));
            weight.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const& choice : LabelWeights)
            {
                weight.Items().Append(box_value(resources::GetString(choice.NameKey)));
            }

            weight.SelectedIndex(0);

            for (int32_t index = 0; index < static_cast<int32_t>(std::size(LabelWeights)); ++index)
            {
                if (working->FontWeight == LabelWeights[index].Weight)
                {
                    weight.SelectedIndex(index);
                    break;
                }
            }

            // ---- the switches ----

            controls::CheckBox italic{};
            italic.Content(box_value(resources::GetString(L"FontItalic")));
            italic.IsChecked(working->Italic);

            controls::CheckBox underline{};
            underline.Content(box_value(resources::GetString(L"FontUnderline")));
            underline.IsChecked(working->Underline);

            controls::CheckBox wrap{};
            wrap.Content(box_value(resources::GetString(L"FontWrap")));
            wrap.IsChecked(working->Wrap);

            controls::StackPanel switches{};
            switches.Orientation(controls::Orientation::Horizontal);
            switches.Spacing(16);
            switches.Children().Append(italic);
            switches.Children().Append(underline);
            switches.Children().Append(wrap);

            // ---- color ----

            controls::TextBox color{};
            color.Header(box_value(resources::GetString(L"FontColorHeader")));
            color.Text(winrt::hstring{ working->Color });
            color.PlaceholderText(resources::GetString(L"FontColorPlaceholder"));

            // ---- the preview ----

            controls::Border previewHost{};
            previewHost.Height(64);
            previewHost.CornerRadius({ 6, 6, 6, 6 });
            previewHost.Padding({ 10, 8, 10, 8 });
            previewHost.Background(xaml::Application::Current().Resources()
                .Lookup(box_value(L"CardBackgroundFillColorSecondaryBrush")).as<media::Brush>());

            controls::TextBlock preview{};
            preview.Text(winrt::hstring{ control->Label.empty()
                ? std::wstring{ resources::GetString(L"FontPreviewFallback") }
                : control->Label });
            preview.TextWrapping(xaml::TextWrapping::Wrap);
            preview.VerticalAlignment(xaml::VerticalAlignment::Center);
            preview.HorizontalAlignment(xaml::HorizontalAlignment::Center);

            previewHost.Child(preview);

            root.Children().Append(family);
            root.Children().Append(size);
            root.Children().Append(weight);
            root.Children().Append(switches);
            root.Children().Append(color);
            root.Children().Append(previewHost);

            dialog.Content(root);

            auto const apply = [=]()
                {
                    auto const familyIndex = family.SelectedIndex();
                    auto const sizeIndex = size.SelectedIndex();
                    auto const weightIndex = weight.SelectedIndex();

                    working->FontFamily = familyIndex > 0 &&
                        familyIndex <= static_cast<int32_t>(std::size(LabelFonts))
                        ? LabelFonts[familyIndex - 1]
                        : L"";

                    working->FontSize = sizeIndex > 0 &&
                        sizeIndex <= static_cast<int32_t>(std::size(LabelSizes))
                        ? LabelSizes[sizeIndex - 1]
                        : 0.0;

                    working->FontWeight = weightIndex >= 0 &&
                        weightIndex < static_cast<int32_t>(std::size(LabelWeights))
                        ? LabelWeights[weightIndex].Weight
                        : 0;

                    working->Italic = italic.IsChecked() != nullptr && italic.IsChecked().Value();
                    working->Underline = underline.IsChecked() != nullptr && underline.IsChecked().Value();
                    working->Wrap = wrap.IsChecked() != nullptr && wrap.IsChecked().Value();
                    working->Color = std::wstring{ color.Text() };

                    // The preview is the only honest way to judge a font pairing, and it costs
                    // nothing to keep in step.
                    preview.FontSize(working->FontSize > 0.0 ? working->FontSize : 12.0);

                    preview.FontFamily(working->FontFamily.empty()
                        ? media::FontFamily{ L"Segoe UI Variable Text" }
                        : media::FontFamily{ working->FontFamily });

                    preview.FontWeight(winrt::Windows::UI::Text::FontWeight{
                        static_cast<uint16_t>(working->FontWeight > 0 ? working->FontWeight : 400) });

                    preview.FontStyle(working->Italic
                        ? winrt::Windows::UI::Text::FontStyle::Italic
                        : winrt::Windows::UI::Text::FontStyle::Normal);

                    preview.TextDecorations(working->Underline
                        ? winrt::Windows::UI::Text::TextDecorations::Underline
                        : winrt::Windows::UI::Text::TextDecorations::None);

                    glass::ThemeColor parsed{};

                    preview.Foreground(!working->Color.empty() && glass::TryParseColor(working->Color, parsed)
                        ? media::SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(
                            parsed.A, parsed.R, parsed.G, parsed.B))
                        : xaml::Application::Current().Resources()
                            .Lookup(box_value(L"TextFillColorPrimaryBrush")).as<media::Brush>());
                };

            family.SelectionChanged([=](auto&&, auto&&) { apply(); });
            size.SelectionChanged([=](auto&&, auto&&) { apply(); });
            weight.SelectionChanged([=](auto&&, auto&&) { apply(); });
            italic.Checked([=](auto&&, auto&&) { apply(); });
            italic.Unchecked([=](auto&&, auto&&) { apply(); });
            underline.Checked([=](auto&&, auto&&) { apply(); });
            underline.Unchecked([=](auto&&, auto&&) { apply(); });
            wrap.Checked([=](auto&&, auto&&) { apply(); });
            wrap.Unchecked([=](auto&&, auto&&) { apply(); });
            color.TextChanged([=](auto&&, auto&&) { apply(); });

            apply();

            auto const result = co_await dialog.ShowAsync();

            if (result == controls::ContentDialogResult::None)
            {
                co_return;
            }

            if (result == controls::ContentDialogResult::Secondary)
            {
                // Back to the theme, keeping the width, which is a layout decision rather than a
                // typographic one.
                auto const width = working->WidthPercent;

                *working = glass::LabelStyle{};
                working->WidthPercent = width;
            }

            if (m_editor.SetControlLabelStyle(controlId, *working))
            {
                RebuildSurface();
                RefreshInspector();
                MarkChanged();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the label font.")
    }
}
