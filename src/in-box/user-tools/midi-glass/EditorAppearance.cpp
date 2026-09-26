// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Layout settings: the rail, and the theme editor behind its first item.
//
// A theme is six hue slots, a deck and a set of control defaults, and a control stores a slot
// number rather than a color. That is what makes swapping a theme a six color operation instead
// of a redesign, and it is why this screen shows how many controls are sitting on each slot: a
// change here is about to touch all of them.
//
// Two rules the design settled and this screen keeps:
//
//   - Contrast is MEASURED, not guessed. Every slot is checked against the deck and called out
//     before the layout goes near a stage.
//   - Not every theme has to be equally accessible. Some ask for ordinary color vision. The
//     honest thing is to measure it, say what the theme costs, and leave high contrast one press
//     away - not to wash a palette out until it no longer looks like the thing it is named after.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "ThemeStore.h"
#include "LayoutStore.h"
#include "DeckBrush.h"

#include <algorithm>
#include <cmath>

namespace resources = ::midiglass::resources;
namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

        // The comp's gallery: three cards across, 9 px apart, each one a 62 px painted miniature
        // of its own deck with its name under it.
        constexpr int32_t GalleryColumns = 3;
        constexpr double GalleryCardWidth = 152.0;
        constexpr double GalleryGap = 9.0;
        constexpr double GalleryPreviewHeight = 62.0;

        // The swatch at the head of a slot row.
        constexpr double SlotSwatchSize = 26.0;

        // The live preview page. Its own tiny layout, drawn by the same renderer the surface
        // uses, so what it shows is what the page will do rather than an illustration of it.
        constexpr int32_t PreviewPageWidth = 284;
        constexpr int32_t PreviewPageHeight = 236;

        media::SolidColorBrush BrushFromKey(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources()
                .Lookup(box_value(key)).as<media::SolidColorBrush>();
        }

        winrt::Windows::UI::Color ToWindowsColor(_In_ glass::ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }

        // A name for a color, so a slot row reads "3 · Amber" rather than six hex codes in a
        // column. Worked out from the color itself rather than stored, because a slot is a
        // number in the file and naming it in there would be one more thing to translate.
        wchar_t const* ColorNameKeyFor(_In_ glass::ThemeColor const& color) noexcept
        {
            auto const red = color.R / 255.0;
            auto const green = color.G / 255.0;
            auto const blue = color.B / 255.0;

            auto const highest = (std::max)({ red, green, blue });
            auto const lowest = (std::min)({ red, green, blue });
            auto const span = highest - lowest;

            if (span < 0.10)
            {
                if (highest > 0.80) { return L"ColorNameWhite"; }
                if (highest < 0.20) { return L"ColorNameBlack"; }

                return L"ColorNameGray";
            }

            auto hue = 0.0;

            if (highest == red)
            {
                hue = 60.0 * std::fmod((green - blue) / span, 6.0);
            }
            else if (highest == green)
            {
                hue = 60.0 * ((blue - red) / span + 2.0);
            }
            else
            {
                hue = 60.0 * ((red - green) / span + 4.0);
            }

            if (hue < 0.0)
            {
                hue += 360.0;
            }

            // Fourteen names around the wheel. Enough that two slots a customer can tell apart
            // get different words, and few enough that none of them needs a qualifier.
            if (hue < 12.0)  { return L"ColorNameRed"; }
            if (hue < 26.0)  { return L"ColorNameOrange"; }
            if (hue < 40.0)  { return L"ColorNameAmber"; }
            if (hue < 62.0)  { return L"ColorNameYellow"; }
            if (hue < 82.0)  { return L"ColorNameLime"; }
            if (hue < 140.0) { return L"ColorNameGreen"; }
            if (hue < 165.0) { return L"ColorNameMint"; }

            // 190 rather than 185: the default theme's two blues sit at 187 and 199, and a band
            // edge between them is what stops both slots being called the same thing.
            if (hue < 190.0) { return L"ColorNameTeal"; }
            if (hue < 200.0) { return L"ColorNameCyan"; }
            if (hue < 222.0) { return L"ColorNameAzure"; }
            if (hue < 258.0) { return L"ColorNameBlue"; }
            if (hue < 285.0) { return L"ColorNameViolet"; }
            if (hue < 320.0) { return L"ColorNameMagenta"; }
            if (hue < 348.0) { return L"ColorNamePink"; }

            return L"ColorNameRed";
        }

        // How bright a slot is, 0-100. On a tube theme this is the only thing separating one slot
        // from the next, and the design's ramps are quoted in exactly these numbers.
        int32_t BrightnessPercentOf(_In_ glass::ThemeColor const& color) noexcept
        {
            auto const relative =
                0.2126 * (color.R / 255.0) +
                0.7152 * (color.G / 255.0) +
                0.0722 * (color.B / 255.0);

            return static_cast<int32_t>(std::lround(relative * 100.0));
        }

        shapes::Rectangle MakeSwatchShape(_In_ double size, _In_ glass::ThemeColor const& color)
        {
            shapes::Rectangle swatch{};

            swatch.Width(size);
            swatch.Height(size);
            swatch.RadiusX(5);
            swatch.RadiusY(5);
            swatch.UseLayoutRounding(false);
            swatch.Fill(media::SolidColorBrush(ToWindowsColor(color)));
            swatch.StrokeThickness(1);
            swatch.Stroke(media::SolidColorBrush(
                winrt::Windows::UI::ColorHelper::FromArgb(36, 255, 255, 255)));

            return swatch;
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
            block.Foreground(BrushFromKey(brushKey));

            return block;
        }
    }

    // ---------------------------------------------------------------- the rail

    _Use_decl_annotations_
    void EditorWindow::OnSettingsNavChecked(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (m_updatingSettings)
        {
            return;
        }

        try
        {
            auto const button = sender.try_as<controls::Primitives::ToggleButton>();

            if (button == nullptr)
            {
                return;
            }

            auto const tag = unbox_value_or<winrt::hstring>(button.Tag(), L"");

            ShowSettingsPane(std::wstring{ tag });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to switch settings pane.")
    }

    // A rail item cannot be turned off by clicking it again - there is always somewhere to be.
    _Use_decl_annotations_
    void EditorWindow::OnSettingsNavUnchecked(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (m_updatingSettings)
        {
            return;
        }

        try
        {
            auto const button = sender.try_as<controls::Primitives::ToggleButton>();

            if (button == nullptr)
            {
                return;
            }

            auto const tag = unbox_value_or<winrt::hstring>(button.Tag(), L"");

            if (std::wstring{ tag } == m_settingsPane)
            {
                m_updatingSettings = true;
                button.IsChecked(true);
                m_updatingSettings = false;
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to keep a settings pane selected.")
    }

    _Use_decl_annotations_
    void EditorWindow::ShowSettingsPane(std::wstring const& tag)
    {
        m_settingsPane = tag.empty() ? std::wstring{ L"appearance" } : tag;

        struct PaneEntry
        {
            wchar_t const* Tag;
            xaml::UIElement Pane;
            controls::Primitives::ToggleButton Item;
        };

        PaneEntry const panes[]
        {
            { L"appearance", AppearancePane(), NavAppearance() },
            { L"pages", PagesPane(), NavPages() },
            { L"outputs", OutputsPane(), NavOutputs() },
            { L"behavior", SettingsBehaviorPane(), NavBehavior() },
            { L"name", NamePane(), NavName() },
            { L"accessibility", AccessibilityPane(), NavAccessibility() },
        };

        m_updatingSettings = true;

        for (auto const& entry : panes)
        {
            auto const selected = m_settingsPane == entry.Tag;

            entry.Pane.Visibility(selected ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            entry.Item.IsChecked(selected);
        }

        m_updatingSettings = false;

        if (m_settingsPane == L"appearance")
        {
            RefreshAppearancePane();
        }
        else if (m_settingsPane == L"pages")
        {
            RefreshSettingsPages();
        }
        else if (m_settingsPane == L"outputs")
        {
            RefreshSettingsDevices();
        }
        else if (m_settingsPane == L"behavior")
        {
            RefreshBehaviorPane();
        }
        else if (m_settingsPane == L"name")
        {
            RefreshNamePane();
        }
        else if (m_settingsPane == L"accessibility")
        {
            RefreshAccessibilityPane();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnSettingsExportClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        // Everything a package holds is on disk, so the layout is written first.
        SaveNow();

        ExportLayoutPackage();
    }

    // ------------------------------------------------------------- appearance

    void EditorWindow::RefreshAppearancePane()
    {
        try
        {
            RebuildThemeGallery();
            RebuildThemeSlots();
            RebuildThemeProperties();
            RefreshThemePreview();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the theme.")
    }

    // A theme card is a painted miniature of its own deck, not a row of swatches. Six colors in
    // a line cannot say what a theme is: the difference between Studio Dark and Cathode is the
    // glass, the glow and the scan lines, and none of those is a swatch.
    void EditorWindow::RebuildThemeGallery()
    {
        ThemeGallery().Children().Clear();
        ThemeGallery().ColumnDefinitions().Clear();
        ThemeGallery().RowDefinitions().Clear();

        m_galleryThemes = glass::AllThemes();

        for (int32_t column = 0; column < GalleryColumns; ++column)
        {
            controls::ColumnDefinition definition{};
            definition.Width(xaml::GridLengthHelper::FromValueAndType(GalleryCardWidth, xaml::GridUnitType::Pixel));

            ThemeGallery().ColumnDefinitions().Append(definition);
        }

        auto const rows = (static_cast<int32_t>(m_galleryThemes.size()) + GalleryColumns - 1) / GalleryColumns;

        for (int32_t row = 0; row < rows; ++row)
        {
            controls::RowDefinition definition{};
            definition.Height(xaml::GridLengthHelper::Auto());

            ThemeGallery().RowDefinitions().Append(definition);
        }

        // The selected card is the one the layout is actually drawn with, which is the theme it
        // names only while it has not been edited. An edited layout carries its own.
        auto const& document = m_editor.Document();

        for (size_t index = 0; index < m_galleryThemes.size(); ++index)
        {
            auto const& theme = m_galleryThemes[index];

            auto const selected = !document.HasOwnTheme && theme.Name == document.ThemeName;

            controls::Primitives::ToggleButton card{};

            card.Style(xaml::Application::Current().Resources()
                .Lookup(box_value(L"ThemeCardStyle")).as<xaml::Style>());

            card.Margin({
                0,
                0,
                (index % GalleryColumns) == GalleryColumns - 1 ? 0.0 : GalleryGap,
                GalleryGap });

            card.IsChecked(selected);
            card.Tag(box_value(winrt::hstring{ theme.Name }));

            automation::AutomationProperties::SetName(card, winrt::hstring{ theme.Name });

            controls::Grid inner{};

            controls::RowDefinition previewRow{};
            previewRow.Height(xaml::GridLengthHelper::FromValueAndType(GalleryPreviewHeight, xaml::GridUnitType::Pixel));

            controls::RowDefinition nameRow{};
            nameRow.Height(xaml::GridLengthHelper::Auto());

            inner.RowDefinitions().Append(previewRow);
            inner.RowDefinitions().Append(nameRow);

            auto const preview = BuildThemeCardPreview(theme);

            controls::Grid::SetRow(preview, 0);
            inner.Children().Append(preview);

            controls::Grid nameLine{};
            nameLine.Padding({ 8, 6, 8, 7 });
            nameLine.ColumnSpacing(5.0);
            nameLine.Background(BrushFromKey(L"CardBackgroundFillColorDefaultBrush"));

            controls::ColumnDefinition textColumn{};
            textColumn.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

            controls::ColumnDefinition glyphColumn{};
            glyphColumn.Width(xaml::GridLengthHelper::Auto());

            nameLine.ColumnDefinitions().Append(textColumn);
            nameLine.ColumnDefinitions().Append(glyphColumn);

            auto name = MakeText(winrt::hstring{ theme.Name }, 11.0, L"TextFillColorSecondaryBrush");
            name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

            controls::Grid::SetColumn(name, 0);
            nameLine.Children().Append(name);

            // The check on the selected one, and the accessibility mark on the theme that is
            // there for exactly that reason.
            if (selected || !theme.CautionResourceKey.empty())
            {
                controls::FontIcon glyph{};

                glyph.Glyph(selected ? L"\uE73E" : L"\uE7B3");
                glyph.FontSize(11.0);
                glyph.VerticalAlignment(xaml::VerticalAlignment::Center);
                glyph.Foreground(BrushFromKey(selected
                    ? L"AccentTextFillColorPrimaryBrush"
                    : L"TextFillColorTertiaryBrush"));

                if (!theme.CautionResourceKey.empty())
                {
                    controls::ToolTipService::SetToolTip(
                        glyph, box_value(resources::GetString(theme.CautionResourceKey)));
                }

                controls::Grid::SetColumn(glyph, 1);
                nameLine.Children().Append(glyph);
            }

            controls::Grid::SetRow(nameLine, 1);
            inner.Children().Append(nameLine);

            card.Content(inner);

            card.Checked([weak = get_weak(), name = theme.Name](auto const& sender, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_updatingSettings)
                    {
                        return;
                    }

                    UNREFERENCED_PARAMETER(sender);

                    strong->ChooseThemeByName(name);
                });

            // A gallery is a choice, so the selected card cannot be clicked off.
            card.Unchecked([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_updatingSettings)
                    {
                        return;
                    }

                    if (auto const button = sender.try_as<controls::Primitives::ToggleButton>())
                    {
                        strong->m_updatingSettings = true;
                        button.IsChecked(true);
                        strong->m_updatingSettings = false;
                    }
                });

            controls::Grid::SetColumn(card, static_cast<int32_t>(index % GalleryColumns));
            controls::Grid::SetRow(card, static_cast<int32_t>(index / GalleryColumns));

            ThemeGallery().Children().Append(card);
        }
    }

    // The miniature inside a theme card, drawn from the theme's own numbers: a knob, two faders,
    // a filled pad, an outlined pad and a tall outlined panel, on its own deck.
    _Use_decl_annotations_
    xaml::FrameworkElement EditorWindow::BuildThemeCardPreview(glass::Theme const& theme)
    {
        controls::Grid host{};

        host.Height(GalleryPreviewHeight);

        shapes::Rectangle deck{};
        deck.Fill(glass::MakeDeckBrush(theme.Deck));
        host.Children().Append(deck);

        controls::Canvas canvas{};
        canvas.IsHitTestVisible(false);

        auto const hue = [&theme](size_t slot) noexcept
            {
                return ToWindowsColor(theme.HueSlots[slot]);
            };

        // A ring in the first slot.
        {
            shapes::Ellipse ring{};

            ring.Width(20);
            ring.Height(20);
            ring.StrokeThickness(2);
            ring.Stroke(media::SolidColorBrush(hue(0)));

            controls::Canvas::SetLeft(ring, 11);
            controls::Canvas::SetTop(ring, 12);
            canvas.Children().Append(ring);
        }

        // Two bars in the second.
        for (auto const left : { 42.0, 54.0 })
        {
            shapes::Rectangle bar{};

            bar.Width(5);
            bar.Height(38);
            bar.RadiusX(2);
            bar.RadiusY(2);
            bar.Fill(media::SolidColorBrush(hue(1)));

            controls::Canvas::SetLeft(bar, left);
            controls::Canvas::SetTop(bar, 10);
            canvas.Children().Append(bar);
        }

        // A filled pad in the third and an outlined one in the fourth.
        {
            shapes::Rectangle filled{};

            filled.Width(34);
            filled.Height(16);
            filled.RadiusX(2);
            filled.RadiusY(2);
            filled.Opacity(0.75);
            filled.Fill(media::SolidColorBrush(hue(2)));

            controls::Canvas::SetLeft(filled, 70);
            controls::Canvas::SetTop(filled, 14);
            canvas.Children().Append(filled);

            shapes::Rectangle outlined{};

            outlined.Width(34);
            outlined.Height(14);
            outlined.RadiusX(2);
            outlined.RadiusY(2);
            outlined.StrokeThickness(1);
            outlined.UseLayoutRounding(false);
            outlined.Stroke(media::SolidColorBrush(hue(3)));

            controls::Canvas::SetLeft(outlined, 70);
            controls::Canvas::SetTop(outlined, 34);
            canvas.Children().Append(outlined);
        }

        // A tall panel in the fifth.
        {
            shapes::Rectangle panel{};

            panel.Width(26);
            panel.Height(38);
            panel.RadiusX(2);
            panel.RadiusY(2);
            panel.StrokeThickness(1);
            panel.UseLayoutRounding(false);
            panel.Stroke(media::SolidColorBrush(hue(4)));

            controls::Canvas::SetLeft(panel, 114);
            controls::Canvas::SetTop(panel, 10);
            canvas.Children().Append(panel);
        }

        host.Children().Append(canvas);

        // The tube themes are their overlay as much as they are their palette, so the card wears
        // it too. Without it Cathode and Terminal Green are two dark rectangles.
        glass::ApplyDeckOverlay(canvas, theme, GalleryCardWidth, GalleryPreviewHeight, 1.0);

        return host;
    }

    _Use_decl_annotations_
    void EditorWindow::ChooseThemeByName(std::wstring const& name)
    {
        try
        {
            for (auto const& theme : m_galleryThemes)
            {
                if (theme.Name != name)
                {
                    continue;
                }

                if (m_editor.ChooseTheme(theme))
                {
                    m_theme = theme;

                    MarkChanged();
                    ApplyThemeEverywhere();
                    RefreshAppearancePane();
                }

                return;
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the theme.")
    }

    // Six rows: the swatch, what the color is called, its code, how many controls are sitting on
    // it, and what it measures against the deck.
    void EditorWindow::RebuildThemeSlots()
    {
        ThemeSlotList().Children().Clear();

        auto const measured = glass::MeasureContrast(m_theme);
        auto const& document = m_editor.Document();

        std::array<int32_t, glass::ThemeHueSlotCount> counts{};

        for (auto const& page : document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (control.HueSlot >= 0 && control.HueSlot < glass::ThemeHueSlotCount)
                {
                    counts[static_cast<size_t>(control.HueSlot)]++;
                }
            }
        }

        auto worst = -1;
        auto worstRatio = 0.0;

        // A tube theme's six slots are one color at six brightnesses, so the wheel gives four of
        // them the same word and the list stops telling anybody anything. Where a name repeats,
        // the row carries the brightness too — which is the channel those themes actually ramp.
        std::array<wchar_t const*, glass::ThemeHueSlotCount> nameKeys{};
        std::array<bool, glass::ThemeHueSlotCount> nameRepeats{};

        for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
        {
            nameKeys[static_cast<size_t>(slot)] =
                ColorNameKeyFor(m_theme.HueSlots[static_cast<size_t>(slot)]);
        }

        for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
        {
            for (int32_t other = 0; other < glass::ThemeHueSlotCount; ++other)
            {
                if (other != slot &&
                    std::wcscmp(nameKeys[static_cast<size_t>(slot)],
                                nameKeys[static_cast<size_t>(other)]) == 0)
                {
                    nameRepeats[static_cast<size_t>(slot)] = true;
                    break;
                }
            }
        }

        for (int32_t slot = 0; slot < glass::ThemeHueSlotCount; ++slot)
        {
            auto const& color = m_theme.HueSlots[static_cast<size_t>(slot)];
            auto const& contrast = measured[static_cast<size_t>(slot)];

            if (!contrast.MeetsMinimum && (worst < 0 || contrast.Ratio < worstRatio))
            {
                worst = slot;
                worstRatio = contrast.Ratio;
            }

            controls::Grid row{};

            row.ColumnSpacing(9.0);

            for (auto const width : { 0.0, 96.0, 74.0, 0.0, 0.0 })
            {
                controls::ColumnDefinition definition{};

                definition.Width(width > 0.0
                    ? xaml::GridLengthHelper::FromValueAndType(width, xaml::GridUnitType::Pixel)
                    : xaml::GridLengthHelper::Auto());

                row.ColumnDefinitions().Append(definition);
            }

            // The swatch is the button: clicking a color is how anybody expects to change one.
            controls::Button swatchButton{};

            swatchButton.Padding({ 0, 0, 0, 0 });
            swatchButton.MinWidth(0);
            swatchButton.MinHeight(0);
            swatchButton.BorderThickness({ 0, 0, 0, 0 });
            swatchButton.Background(nullptr);
            swatchButton.Content(MakeSwatchShape(SlotSwatchSize, color));

            automation::AutomationProperties::SetName(swatchButton, resources::FormatString(
                L"ThemeSlotSwatchFormat", slot + 1));

            swatchButton.Click([weak = get_weak(), slot](foundation::IInspectable const& sender, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    strong->ShowSlotColorFlyout(sender.try_as<xaml::FrameworkElement>(), slot);
                });

            controls::Grid::SetColumn(swatchButton, 0);
            row.Children().Append(swatchButton);

            auto const name = std::wstring{ resources::GetString(nameKeys[static_cast<size_t>(slot)]) };

            auto title = MakeText(
                nameRepeats[static_cast<size_t>(slot)]
                    ? resources::FormatString(
                        L"ThemeSlotNameBrightnessFormat",
                        slot + 1,
                        name,
                        BrightnessPercentOf(color))
                    : resources::FormatString(
                        L"ThemeSlotNameFormat",
                        slot + 1,
                        name),
                12.0,
                L"TextFillColorSecondaryBrush");

            title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

            controls::Grid::SetColumn(title, 1);
            row.Children().Append(title);

            auto code = MakeText(
                winrt::hstring{ glass::ColorToText(color) }, 11.0, L"TextFillColorTertiaryBrush");

            code.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas, Courier New" });

            controls::Grid::SetColumn(code, 2);
            row.Children().Append(code);

            auto used = MakeText(
                counts[static_cast<size_t>(slot)] == 1
                    ? resources::GetString(L"ThemeSlotOneControl")
                    : resources::FormatString(L"ThemeSlotControlsFormat", counts[static_cast<size_t>(slot)]),
                10.5,
                L"TextFillColorTertiaryBrush");

            controls::Grid::SetColumn(used, 3);
            row.Children().Append(used);

            // Measured, not guessed, and shown next to the color it is about.
            controls::Border chip{};

            chip.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(3.0));
            chip.Padding({ 6, 1, 6, 2 });
            chip.HorizontalAlignment(xaml::HorizontalAlignment::Right);
            chip.VerticalAlignment(xaml::VerticalAlignment::Center);
            chip.Background(BrushFromKey(contrast.MeetsMinimum
                ? L"SystemFillColorSuccessBackgroundBrush"
                : L"SystemFillColorCautionBackgroundBrush"));

            auto ratio = MakeText(
                resources::FormatString(L"ThemeRatioFormat", std::format(L"{:.1f}", contrast.Ratio)),
                10.5,
                contrast.MeetsMinimum ? L"SystemFillColorSuccessBrush" : L"SystemFillColorCautionBrush");

            ratio.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas, Courier New" });

            chip.Child(ratio);

            controls::Grid::SetColumn(chip, 4);
            row.Children().Append(chip);

            ThemeSlotList().Children().Append(row);
        }

        if (worst >= 0)
        {
            ThemeContrastWarningText().Text(
                resources::FormatString(L"ThemeSlotTooCloseFormat", worst + 1));
            ThemeContrastWarning().Visibility(xaml::Visibility::Visible);
        }
        else
        {
            ThemeContrastWarning().Visibility(xaml::Visibility::Collapsed);
        }

        // What the theme costs, in the picker, where somebody is choosing rather than debugging.
        if (m_theme.CautionResourceKey.empty())
        {
            ThemeCautionText().Visibility(xaml::Visibility::Collapsed);
        }
        else
        {
            ThemeCautionText().Text(resources::GetString(m_theme.CautionResourceKey));
            ThemeCautionText().Visibility(xaml::Visibility::Visible);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::ShowSlotColorFlyout(xaml::FrameworkElement const& anchor, int32_t slot)
    {
        if (anchor == nullptr || slot < 0 || slot >= glass::ThemeHueSlotCount)
        {
            return;
        }

        ShowColorFlyout(
            anchor,
            glass::ColorToText(m_theme.HueSlots[static_cast<size_t>(slot)]),
            false,
            [weak = get_weak(), slot](std::wstring const& code)
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                glass::ThemeColor parsed{};

                if (!glass::TryParseColor(code, parsed))
                {
                    return;
                }

                strong->EditTheme([slot, parsed](glass::Theme& theme)
                    {
                        theme.HueSlots[static_cast<size_t>(slot)] = parsed;
                    });

                strong->RebuildThemeSlots();
            });
    }

    // ------------------------------------------------------- applying an edit

    _Use_decl_annotations_
    void EditorWindow::EditTheme(std::function<void(glass::Theme&)> const& change)
    {
        try
        {
            change(m_theme);

            // From here on the layout carries the theme itself. There is no file anywhere that
            // says what it now is, so the only place it can live is inside the layout - which is
            // also what makes a layout sent to somebody look the way it was built.
            m_theme.IsBuiltIn = false;

            m_editor.SetOwnTheme(m_theme);

            MarkChanged();
            ApplyThemeEverywhere();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the theme.")
    }

    void EditorWindow::ApplyThemeEverywhere()
    {
        try
        {
            UpdateDeckBrushes();
            RebuildSurface();
            RefreshThemePreview();

            // The palette a control is picked from is the theme's, so it has to follow the
            // theme rather than stay on the one the window opened with.
            RefreshHueSwatches();

            // The gallery's selected card and the slot rows both read the theme.
            if (m_settingsPane == L"accessibility")
            {
                RefreshAccessibilityPane();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to repaint the surface for the new theme.")
    }

    // One page holding one of most things, drawn by the same renderer the surface uses. An
    // illustration would eventually disagree with the renderer; this cannot.
    void EditorWindow::RefreshThemePreview()
    {
        try
        {
            ThemePreviewDeck().Fill(glass::MakeDeckBrush(m_theme.Deck));

            glass::LayoutDocument preview{};

            preview.PageWidth = PreviewPageWidth;
            preview.PageHeight = PreviewPageHeight;

            glass::Page page{};

            struct Item
            {
                glass::ControlKind Kind;
                double X;
                double Y;
                double Width;
                double Height;
                int32_t Slot;
                double Value;
                wchar_t const* Label;
            };

            Item const items[]
            {
                { glass::ControlKind::Knob,    14,  44,  46, 46, 0, 0.52, L"" },
                { glass::ControlKind::Knob,    72,  44,  46, 46, 4, 0.30, L"" },
                { glass::ControlKind::Fader,  140,  40,  30, 94, 1, 0.66, L"" },
                { glass::ControlKind::Fader,  178,  40,  30, 94, 1, 0.41, L"" },
                { glass::ControlKind::Meter,  216,  40,  16, 94, 1, 0.74, L"" },
                { glass::ControlKind::Toggle,  14, 108,  50, 30, 2, 1.00, L"On" },
                { glass::ControlKind::Button,  70, 108,  50, 30, 2, 0.00, L"Off" },
                { glass::ControlKind::XYPad,   14, 150, 106, 72, 3, 0.60, L"" },
                { glass::ControlKind::Pad,    140, 150,  60, 32, 5, 1.00, L"" },
                { glass::ControlKind::Readout, 206, 150, 62, 32, 0, 0.00, L"128.0" },
            };

            for (auto const& item : items)
            {
                glass::Control control{};

                control.Id = glass::LayoutDocument::NewId();
                control.Kind = item.Kind;
                control.X = item.X;
                control.Y = item.Y;
                control.Width = item.Width;
                control.Height = item.Height;
                control.HueSlot = item.Slot;
                control.DefaultValue = item.Value;
                control.Label = item.Label;
                control.LabelPlaced = glass::LabelPlacementOverride::None;

                page.Controls.push_back(control);
            }

            preview.Pages.push_back(std::move(page));

            ThemePreviewCanvas().Width(PreviewPageWidth);
            ThemePreviewCanvas().Height(PreviewPageHeight);

            m_themePreview.Teardown();
            m_themePreview.Build(ThemePreviewCanvas(), preview, m_theme, 0);

            for (size_t index = 0; index < m_themePreview.ItemCount(); ++index)
            {
                m_themePreview.SetValue(index, preview.Pages[0].Controls[index].DefaultValue);

                if (auto const element = m_themePreview.ElementAt(index))
                {
                    element.IsHitTestVisible(false);
                }
            }

            // A tube theme is its overlay as much as it is its palette.
            glass::ApplyDeckOverlay(
                ThemePreviewCanvas(), m_theme, PreviewPageWidth, PreviewPageHeight, 1.0);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to draw the theme preview.")
    }
}
