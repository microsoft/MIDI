// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Every number a theme is made of, as a row.
//
// The rule this file exists to keep: a theme property with no row here is a property nobody can
// reach. The shipped themes between them use all of it - Bone's warm shadow, Bigwig's neutral
// rim and lamp ring, the tonal themes' wash at rest, and the three tube themes' scan lines, corner
// fall-off, faceplate reflection, resting glow and bloom color. If a property is added to the
// model, it gets a row in here in the same change.
//
// Two of them are structural rather than decorative, and the editor says so instead of letting
// somebody quietly wreck a theme:
//
//   - On Bone the elevation shadow is the ONLY thing separating a control from the deck. A warm
//     white plate measures 1.23 : 1 against a bone deck, so at zero the layout disappears.
//   - On a tube theme the resting glow does the same job, because a raster box measures 1.11 : 1
//     to 1.22 : 1 against the middle of its own glass.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "ThemeStore.h"
#include "LayoutStore.h"

#include <algorithm>
#include <cmath>

namespace resources = ::midiglass::resources;
namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

namespace winrt::midiglass::implementation
{
    namespace
    {
        namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

        media::SolidColorBrush ThemeBrush(_In_ wchar_t const* key)
        {
            return xaml::Application::Current().Resources()
                .Lookup(box_value(key)).as<media::SolidColorBrush>();
        }

        controls::TextBlock RowLabel(_In_ wchar_t const* key)
        {
            controls::TextBlock text{};

            text.Text(resources::GetString(key));
            text.FontSize(12.0);
            text.Width(118.0);
            text.TextWrapping(xaml::TextWrapping::Wrap);
            text.VerticalAlignment(xaml::VerticalAlignment::Center);
            text.Foreground(ThemeBrush(L"TextFillColorSecondaryBrush"));

            return text;
        }

        controls::Grid RowHost()
        {
            controls::Grid row{};

            row.ColumnSpacing(9.0);
            row.Margin({ 0, 0, 0, 7 });

            controls::ColumnDefinition labelColumn{};
            labelColumn.Width(xaml::GridLengthHelper::FromValueAndType(118.0, xaml::GridUnitType::Pixel));

            controls::ColumnDefinition fieldColumn{};
            fieldColumn.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

            controls::ColumnDefinition trailingColumn{};
            trailingColumn.Width(xaml::GridLengthHelper::Auto());

            row.ColumnDefinitions().Append(labelColumn);
            row.ColumnDefinitions().Append(fieldColumn);
            row.ColumnDefinitions().Append(trailingColumn);

            return row;
        }

        controls::TextBlock NumberText(_In_ winrt::hstring const& text)
        {
            controls::TextBlock number{};

            number.Text(text);
            number.FontSize(11.0);
            number.MinWidth(46.0);
            number.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas, Courier New" });
            number.TextAlignment(xaml::TextAlignment::Right);
            number.VerticalAlignment(xaml::VerticalAlignment::Center);
            number.Foreground(ThemeBrush(L"TextFillColorTertiaryBrush"));

            return number;
        }

        controls::TextBlock GroupHeading(_In_ wchar_t const* key, _In_ bool first)
        {
            controls::TextBlock heading{};

            heading.Text(resources::GetString(key));
            heading.FontSize(13.5);
            heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            heading.Margin({ 0, first ? 0.0 : 16.0, 0, 7 });

            return heading;
        }

        // The caption under a row that is carrying something structural, so nobody turns off the
        // one thing holding the theme together without being told what it does.
        controls::TextBlock RowNote(_In_ wchar_t const* key)
        {
            controls::TextBlock note{};

            note.Text(resources::GetString(key));
            note.FontSize(11.0);
            note.LineHeight(16.0);
            note.TextWrapping(xaml::TextWrapping::Wrap);
            note.Margin({ 0, -3, 0, 9 });
            note.Foreground(ThemeBrush(L"TextFillColorTertiaryBrush"));

            return note;
        }
    }

    // A slider row. The number beside it is the value in its own units, because "62" is what
    // somebody types into another theme and "0.62" is not.
    namespace
    {
        // A slider whose floor means "follow the row above" says so in words. A bare "-1 %" in
        // that column reads as a broken value rather than as a choice. Every other slider that
        // goes below zero means what it says.
        winrt::hstring SliderNumberText(_In_opt_ wchar_t const* unitKey, _In_ double value)
        {
            auto const rounded = static_cast<int32_t>(std::lround(value));

            if (rounded < 0 && unitKey != nullptr && std::wstring_view{ unitKey } == L"ThemeUnitPercentOrFollow")
            {
                return resources::GetString(L"ThemeFollowsFillAtRest");
            }

            return unitKey == nullptr
                ? winrt::hstring{ std::format(L"{}", rounded) }
                : resources::FormatString(unitKey, rounded);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::AddThemeSliderRow(
        wchar_t const* labelKey,
        wchar_t const* unitKey,
        double smallest,
        double largest,
        double value,
        std::function<void(double)> const& apply)
    {
        auto row = RowHost();

        auto label = RowLabel(labelKey);
        controls::Grid::SetColumn(label, 0);
        row.Children().Append(label);

        controls::Slider slider{};

        slider.Minimum(smallest);
        slider.Maximum(largest);
        slider.Value(std::clamp(value, smallest, largest));
        slider.StepFrequency(1.0);
        slider.VerticalAlignment(xaml::VerticalAlignment::Center);

        // The stock tick and header take twice the height the comp draws, and this column has
        // eight groups of rows to fit in it.
        slider.MinWidth(0.0);
        slider.Margin({ 0, -4, 0, -4 });

        automation::AutomationProperties::SetName(slider, resources::GetString(labelKey));

        auto number = NumberText(SliderNumberText(unitKey, value));

        slider.ValueChanged([weak = get_weak(), number, unitKey, apply](foundation::IInspectable const& sender, auto&&)
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_updatingSettings)
                {
                    return;
                }

                auto const current = sender.as<controls::Slider>().Value();

                number.Text(SliderNumberText(unitKey, current));

                apply(current);
            });

        controls::Grid::SetColumn(slider, 1);
        row.Children().Append(slider);

        controls::Grid::SetColumn(number, 2);
        row.Children().Append(number);

        ThemePropertyPanel().Children().Append(row);
    }

    _Use_decl_annotations_
    void EditorWindow::AddThemeComboRow(
        wchar_t const* labelKey,
        std::vector<wchar_t const*> const& itemKeys,
        int32_t selected,
        std::function<void(int32_t)> const& apply)
    {
        auto row = RowHost();

        auto label = RowLabel(labelKey);
        controls::Grid::SetColumn(label, 0);
        row.Children().Append(label);

        controls::ComboBox combo{};

        combo.Height(30.0);
        combo.MinWidth(0.0);
        combo.FontSize(12.0);
        combo.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(4.0));
        combo.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

        for (auto const* key : itemKeys)
        {
            combo.Items().Append(box_value(resources::GetString(key)));
        }

        combo.SelectedIndex(std::clamp<int32_t>(selected, 0, static_cast<int32_t>(itemKeys.size()) - 1));

        automation::AutomationProperties::SetName(combo, resources::GetString(labelKey));

        combo.SelectionChanged([weak = get_weak(), apply](foundation::IInspectable const& sender, auto&&)
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_updatingSettings)
                {
                    return;
                }

                // Guarded against -1: a rebuild clears the selection on the way through, and an
                // unguarded handler turns every refresh into an edit.
                auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                if (index >= 0)
                {
                    apply(index);
                }
            });

        controls::Grid::SetColumn(combo, 1);
        row.Children().Append(combo);

        ThemePropertyPanel().Children().Append(row);
    }

    // A color code and the swatch button beside it. The box stays, because a code pasted out of
    // a brand guide is the fastest way in and is the only thing a screen reader can read back.
    _Use_decl_annotations_
    void EditorWindow::AddThemeColorRow(
        wchar_t const* labelKey,
        glass::ThemeColor const& color,
        bool allowEmpty,
        std::function<void(std::wstring const&)> const& apply)
    {
        auto row = RowHost();

        auto label = RowLabel(labelKey);
        controls::Grid::SetColumn(label, 0);
        row.Children().Append(label);

        controls::Grid field{};
        field.ColumnSpacing(6.0);

        controls::ColumnDefinition boxColumn{};
        boxColumn.Width(xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star));

        controls::ColumnDefinition buttonColumn{};
        buttonColumn.Width(xaml::GridLengthHelper::Auto());

        field.ColumnDefinitions().Append(boxColumn);
        field.ColumnDefinitions().Append(buttonColumn);

        controls::TextBox box{};

        box.Height(30.0);
        box.MinWidth(0.0);
        box.FontSize(12.0);
        box.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(4.0));
        box.Text(allowEmpty && color.A == 0 ? L"" : winrt::hstring{ glass::ColorToText(color) });

        automation::AutomationProperties::SetName(box, resources::GetString(labelKey));

        box.LostFocus([weak = get_weak(), box, apply](auto&&, auto&&)
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_updatingSettings)
                {
                    return;
                }

                apply(std::wstring{ box.Text() });
            });

        controls::Grid::SetColumn(box, 0);
        field.Children().Append(box);

        controls::Button button{};

        automation::AutomationProperties::SetName(button, resources::FormatString(
            L"ThemePickColorFormat", std::wstring{ resources::GetString(labelKey) }));

        AttachColorPicker(button, box, allowEmpty,
            [weak = get_weak(), box, apply]()
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_updatingSettings)
                {
                    return;
                }

                apply(std::wstring{ box.Text() });
            });

        controls::Grid::SetColumn(button, 1);
        field.Children().Append(button);

        controls::Grid::SetColumn(field, 1);
        row.Children().Append(field);

        ThemePropertyPanel().Children().Append(row);
    }

    _Use_decl_annotations_
    void EditorWindow::AddThemeSwitchRow(
        wchar_t const* labelKey,
        bool on,
        std::function<void(bool)> const& apply)
    {
        auto row = RowHost();

        auto label = RowLabel(labelKey);
        controls::Grid::SetColumn(label, 0);
        row.Children().Append(label);

        controls::ToggleSwitch toggle{};

        toggle.IsOn(on);
        toggle.OnContent(box_value(L""));
        toggle.OffContent(box_value(L""));
        toggle.MinWidth(0.0);
        toggle.Margin({ 0, -4, 0, -6 });

        automation::AutomationProperties::SetName(toggle, resources::GetString(labelKey));

        toggle.Toggled([weak = get_weak(), apply](foundation::IInspectable const& sender, auto&&)
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_updatingSettings)
                {
                    return;
                }

                apply(sender.as<controls::ToggleSwitch>().IsOn());
            });

        controls::Grid::SetColumn(toggle, 1);
        row.Children().Append(toggle);

        ThemePropertyPanel().Children().Append(row);
    }

    void EditorWindow::AddThemeGroupHeading(_In_ wchar_t const* key, _In_ bool first)
    {
        ThemePropertyPanel().Children().Append(GroupHeading(key, first));
    }

    void EditorWindow::AddThemeNote(_In_ wchar_t const* key)
    {
        ThemePropertyPanel().Children().Append(RowNote(key));
    }

    // ------------------------------------------------------------ the panel

    void EditorWindow::RebuildThemeProperties()
    {
        m_updatingSettings = true;

        ThemePropertyPanel().Children().Clear();

        auto const edit = [weak = get_weak()](std::function<void(glass::Theme&)> change)
            {
                if (auto strong = weak.get())
                {
                    strong->EditTheme(change);
                }
            };

        auto const color = [](std::wstring const& code, glass::ThemeColor& target, bool allowEmpty)
            {
                glass::ThemeColor parsed{};

                if (glass::TryParseColor(code, parsed))
                {
                    target = parsed;
                    return true;
                }

                if (allowEmpty && code.empty())
                {
                    target = {};
                    return true;
                }

                return false;
            };

        // ---- the deck ----

        AddThemeGroupHeading(L"ThemeGroupDeck", true);

        AddThemeComboRow(L"ThemeRowBackground",
            { L"ThemeDeckOneColor", L"ThemeDeckTwoColors", L"ThemeDeckPicture" },
            static_cast<int32_t>(m_theme.Deck.Kind),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme) { theme.Deck.Kind = static_cast<glass::DeckKind>(index); });
            });

        // The picture a Picture deck is drawn with. Choosing one switches the deck over to it.
        {
            auto row = RowHost();

            auto label = RowLabel(L"ThemeRowDeckPicture");
            controls::Grid::SetColumn(label, 0);
            row.Children().Append(label);

            controls::TextBlock name{};
            name.Text(m_theme.Deck.ImageFileName.empty()
                ? resources::GetString(L"ThemeDeckPictureNone")
                : winrt::hstring{ m_theme.Deck.ImageFileName });
            name.FontSize(12.0);
            name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            name.VerticalAlignment(xaml::VerticalAlignment::Center);
            controls::Grid::SetColumn(name, 1);
            row.Children().Append(name);

            // The panel is rebuilt after either button, so the combo above shows what changed.
            // Not from inside the click: that would take the button away under its own event.
            auto const rebuildSoon = [](winrt::weak_ref<EditorWindow> const& weak)
                {
                    if (auto strong = weak.get())
                    {
                        strong->DispatcherQueue().TryEnqueue([weak]()
                            {
                                if (auto again = weak.get())
                                {
                                    again->RebuildThemeProperties();
                                }
                            });
                    }
                };

            controls::Button choose{};
            choose.Content(box_value(resources::GetString(L"ThemeDeckPictureChoose")));
            choose.FontSize(12.0);
            automation::AutomationProperties::SetName(choose, resources::GetString(L"ThemeDeckPictureChooseName"));

            choose.Click([weak = get_weak(), rebuildSoon](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    auto const picked = strong->PickBackgroundImageFile(true);

                    if (picked.empty())
                    {
                        return;
                    }

                    auto const stored = glass::CopyDeckImageToThemes(picked);

                    if (stored.empty())
                    {
                        return;
                    }

                    strong->EditTheme([stored](glass::Theme& theme)
                        {
                            theme.Deck.ImageFileName = stored;
                            theme.Deck.Kind = glass::DeckKind::Image;
                        });

                    rebuildSoon(weak);
                });

            controls::Button remove{};
            remove.Content(box_value(resources::GetString(L"ThemeDeckPictureRemove")));
            remove.FontSize(12.0);
            remove.IsEnabled(!m_theme.Deck.ImageFileName.empty());
            automation::AutomationProperties::SetName(remove, resources::GetString(L"ThemeDeckPictureRemoveName"));

            // Only the reference goes. The file stays in the themes folder, because another
            // theme may be using it.
            remove.Click([weak = get_weak(), rebuildSoon](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    strong->EditTheme([](glass::Theme& theme)
                        {
                            theme.Deck.ImageFileName.clear();

                            if (theme.Deck.Kind == glass::DeckKind::Image)
                            {
                                theme.Deck.Kind = glass::DeckKind::SolidColor;
                            }
                        });

                    rebuildSoon(weak);
                });

            controls::StackPanel buttons{};
            buttons.Orientation(controls::Orientation::Horizontal);
            buttons.Spacing(6.0);
            buttons.Children().Append(choose);
            buttons.Children().Append(remove);
            controls::Grid::SetColumn(buttons, 2);
            row.Children().Append(buttons);

            ThemePropertyPanel().Children().Append(row);
        }

        AddThemeNote(L"ThemeDeckPictureNote");

        AddThemeColorRow(L"ThemeRowDeckTop", m_theme.Deck.Color, false,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.Deck.Color, false); });
            });

        AddThemeColorRow(L"ThemeRowDeckFloor", m_theme.Deck.GradientEndColor, false,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.Deck.GradientEndColor, false); });
            });

        // ---- control defaults, the four the comp draws ----

        AddThemeGroupHeading(L"ThemeGroupControls", false);

        AddThemeSliderRow(L"ThemeRowCornerRounding", L"ThemeUnitPixels", 0, 32, m_theme.CornerRadius,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.CornerRadius = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowGlassTint", L"ThemeUnitPercent", 0, 100, m_theme.GlassTintPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.GlassTintPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeColorRow(L"ThemeRowGlassColor", m_theme.GlassColor, false,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.GlassColor, false); });
            });

        AddThemeSliderRow(L"ThemeRowGlow", L"ThemeUnitPercent", 0, 100, m_theme.GlowStrength,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.GlowStrength = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowFillAtRest", L"ThemeUnitPercent", 0, 100,
            std::lround(m_theme.FillAtRest * 100.0),
            [edit](double value)
            {
                edit([value](glass::Theme& theme) { theme.FillAtRest = value / 100.0; });
            });

        // Minus one is "follow the row above", which is what every theme did before a panel
        // arrived with solid tabs and bare knobs on it. The slider reaches it as its bottom
        // step rather than needing a switch beside it.
        AddThemeSliderRow(L"ThemeRowSwitchFillAtRest", L"ThemeUnitPercentOrFollow", -1, 100,
            m_theme.SwitchFillAtRest < 0.0 ? -1 : std::lround(m_theme.SwitchFillAtRest * 100.0),
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.SwitchFillAtRest = value < 0.0 ? -1.0 : value / 100.0; });
            });

        AddThemeSliderRow(L"ThemeRowPadFillAtRest", L"ThemeUnitPercentOrFollow", -1, 100,
            m_theme.PadFillAtRest < 0.0 ? -1 : std::lround(m_theme.PadFillAtRest * 100.0),
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.PadFillAtRest = value < 0.0 ? -1.0 : value / 100.0; });
            });

        AddThemeSliderRow(L"ThemeRowFillWhenOn", L"ThemeUnitPercent", 0, 100, m_theme.FillWhenOnPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.FillWhenOnPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeNote(L"ThemeFillWhenOnNote");

        AddThemeSliderRow(L"ThemeRowPadFillWhenOn", L"ThemeUnitPercentOrFollow", -1, 100, m_theme.PadFillWhenOnPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.PadFillWhenOnPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeNote(L"ThemePadNote");

        AddThemeSliderRow(L"ThemeRowTouchFill", L"ThemeUnitPercent", 0, 100, m_theme.TouchFillPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.TouchFillPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeComboRow(L"ThemeRowLabels",
            { L"ThemeLabelsInside", L"ThemeLabelsBelow", L"ThemeLabelsNone", L"ThemeLabelsAbove" },
            static_cast<int32_t>(m_theme.Labels),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme)
                    { theme.Labels = static_cast<glass::LabelPlacement>(index); });
            });

        AddThemeComboRow(L"ThemeRowSectionHeader",
            { L"ThemeSectionCaption", L"ThemeSectionFilledBar", L"ThemeSectionNotched", L"ThemeSectionCentered" },
            static_cast<int32_t>(m_theme.SectionHeader),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme)
                    { theme.SectionHeader = static_cast<glass::SectionHeaderStyle>(index); });
            });

        AddThemeSwitchRow(L"ThemeRowSectionNameInHue", m_theme.SectionNameInHue,
            [edit](bool on)
            {
                edit([on](glass::Theme& theme) { theme.SectionNameInHue = on; });
            });

        AddThemeSwitchRow(L"ThemeRowNamesInside", m_theme.NamesInsideSwitches,
            [edit](bool on)
            {
                edit([on](glass::Theme& theme) { theme.NamesInsideSwitches = on; });
            });

        AddThemeColorRow(L"ThemeRowNeutralColor", m_theme.NeutralColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.NeutralColor, true); });
            });

        AddThemeNote(L"ThemeNeutralColorNote");

        AddThemeColorRow(L"ThemeRowInk", m_theme.InkColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.InkColor, true); });
            });

        AddThemeNote(L"ThemeInkNote");

        // ---- the plate ----

        AddThemeGroupHeading(L"ThemeGroupPlate", false);

        AddThemeColorRow(L"ThemeRowPlateColor", m_theme.PlateColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.PlateColor, true); });
            });

        AddThemeColorRow(L"ThemeRowPlateBottom", m_theme.PlateEndColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.PlateEndColor, true); });
            });

        AddThemeSliderRow(L"ThemeRowSheen", L"ThemeUnitPercent", 0, 100, m_theme.PlateSheenPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.PlateSheenPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeColorRow(L"ThemeRowSheenColor", m_theme.PlateSheenColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.PlateSheenColor, true); });
            });

        AddThemeSliderRow(L"ThemeRowPlateShade", L"ThemeUnitPercent", 0, 100, m_theme.PlateShadePercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.PlateShadePercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowPlateHighlight", L"ThemeUnitPercent", 0, 100, m_theme.PlateHighlightPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.PlateHighlightPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowElevation", L"ThemeUnitPercent", 0, 100, m_theme.PlateElevation,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.PlateElevation = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowShadowReach", L"ThemeUnitPixels", 0, 32, m_theme.ShadowSpread,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.ShadowSpread = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeColorRow(L"ThemeRowShadowColor", m_theme.ShadowColor, false,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.ShadowColor, false); });
            });

        // The one warning this screen owes somebody: on a theme whose plate has no value
        // difference from its deck, the shadow is the whole structure.
        if (PlateNeedsItsShadow())
        {
            AddThemeNote(L"ThemeShadowIsStructure");
        }

        // ---- the rim ----

        AddThemeGroupHeading(L"ThemeGroupRim", false);

        AddThemeComboRow(L"ThemeRowRimFrom",
            { L"ThemeRimControlColor", L"ThemeRimNeutral", L"ThemeRimNone" },
            static_cast<int32_t>(m_theme.Rim),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme)
                    { theme.Rim = static_cast<glass::RimSource>(index); });
            });

        AddThemeSliderRow(L"ThemeRowRimStrength", L"ThemeUnitPercent", 0, 100, m_theme.RimStrengthPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.RimStrengthPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowSwitchRimStrength", L"ThemeUnitPercentOrFollow", -1, 100,
            m_theme.SwitchRimStrengthPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.SwitchRimStrengthPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeNote(L"ThemeSwitchRimNote");

        AddThemeColorRow(L"ThemeRowNeutralRim", m_theme.NeutralRimColor, false,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.NeutralRimColor, false); });
            });

        // ---- the value ----

        AddThemeGroupHeading(L"ThemeGroupValue", false);

        AddThemeColorRow(L"ThemeRowTrack", m_theme.TrackColor, false,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.TrackColor, false); });
            });

        AddThemeColorRow(L"ThemeRowArcTrack", m_theme.ArcTrackColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.ArcTrackColor, true); });
            });

        AddThemeNote(L"ThemeArcTrackNote");

        AddThemeSliderRow(L"ThemeRowArcTrackHue", L"ThemeUnitPercent", 0, 100, m_theme.ArcTrackHuePercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.ArcTrackHuePercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeNote(L"ThemeArcTrackHueNote");

        AddThemeSwitchRow(L"ThemeRowArcGlow", m_theme.ArcGlow,
            [edit](bool on)
            {
                edit([on](glass::Theme& theme) { theme.ArcGlow = on; });
            });

        AddThemeComboRow(L"ThemeRowValueStrip",
            { L"ThemeStripBottom", L"ThemeStripTop", L"ThemeStripNone" },
            static_cast<int32_t>(m_theme.ValueStrip),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme)
                    { theme.ValueStrip = static_cast<glass::ValueStripPlacement>(index); });
            });

        AddThemeComboRow(L"ThemeRowIndicator",
            { L"ThemeIndicatorArc", L"ThemeIndicatorLamps" },
            static_cast<int32_t>(m_theme.ValueIndicator),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme)
                    { theme.ValueIndicator = static_cast<glass::ValueIndicatorStyle>(index); });
            });

        AddThemeSliderRow(L"ThemeRowLampCount", nullptr, 2, 64, m_theme.LampCount,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.LampCount = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowSmallestRing", L"ThemeUnitPixels", 8, 160, m_theme.MinimumLampRingSize,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.MinimumLampRingSize = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowValueFade", L"ThemeUnitPercent", 0, 100,
            std::lround(m_theme.PipeFalloff * 100.0),
            [edit](double value)
            {
                edit([value](glass::Theme& theme) { theme.PipeFalloff = value / 100.0; });
            });

        AddThemeSwitchRow(L"ThemeRowFadeToLight", m_theme.ValueFadesToLight,
            [edit](bool on)
            {
                edit([on](glass::Theme& theme) { theme.ValueFadesToLight = on; });
            });

        AddThemeComboRow(L"ThemeRowCap",
            { L"ThemeCapNone", L"ThemeCapNeutral", L"ThemeCapControlColor" },
            static_cast<int32_t>(m_theme.Thumb),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme)
                    { theme.Thumb = static_cast<glass::ThumbStyle>(index); });
            });

        AddThemeColorRow(L"ThemeRowCapTop", m_theme.ThumbColor, false,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.ThumbColor, false); });
            });

        AddThemeColorRow(L"ThemeRowCapBottom", m_theme.ThumbEndColor, false,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.ThumbEndColor, false); });
            });

        AddThemeColorRow(L"ThemeRowCapLine", m_theme.CapLineColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.CapLineColor, true); });
            });

        AddThemeColorRow(L"ThemeRowPointer", m_theme.PointerColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.PointerColor, true); });
            });

        AddThemeNote(L"ThemePointerNote");

        AddThemeColorRow(L"ThemeRowValueColor", m_theme.ValueColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.ValueColor, true); });
            });

        AddThemeNote(L"ThemeValueColorNote");

        AddThemeComboRow(L"ThemeRowFaderPlate",
            { L"ThemeFaderPlateFull", L"ThemeFaderPlateStrip", L"ThemeFaderPlateNone", L"ThemeFaderPlateFrame" },
            static_cast<int32_t>(m_theme.FaderPlate),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme)
                    { theme.FaderPlate = static_cast<glass::FaderPlateStyle>(index); });
            });

        AddThemeSliderRow(L"ThemeRowFaderFill", L"ThemeUnitPercent", 0, 100, m_theme.FaderFillPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.FaderFillPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowFaderScale", L"ThemeUnitPercent", 0, 100, m_theme.FaderScalePercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.FaderScalePercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeNote(L"ThemeFaderScaleNote");

        AddThemeSliderRow(L"ThemeRowThumbShadow", L"ThemeUnitPercent", 0, 100, m_theme.ThumbShadowPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.ThumbShadowPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSwitchRow(L"ThemeRowCapLineWide", m_theme.CapLineWide,
            [edit](bool on)
            {
                edit([on](glass::Theme& theme) { theme.CapLineWide = on; });
            });

        AddThemeSliderRow(L"ThemeRowRecessShade", L"ThemeUnitPercent", 0, 100, m_theme.RecessShadePercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.RecessShadePercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeColorRow(L"ThemeRowWellColor", m_theme.WellColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.WellColor, true); });
            });

        AddThemeNote(L"ThemeWellNote");

        // ---- knobs ----

        AddThemeGroupHeading(L"ThemeGroupKnobs", false);

        AddThemeColorRow(L"ThemeRowKnobFace", m_theme.KnobFaceColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.KnobFaceColor, true); });
            });

        AddThemeColorRow(L"ThemeRowKnobFaceEnd", m_theme.KnobFaceEndColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.KnobFaceEndColor, true); });
            });

        AddThemeNote(L"ThemeKnobFaceNote");

        AddThemeColorRow(L"ThemeRowKnobCap", m_theme.KnobCapColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.KnobCapColor, true); });
            });

        AddThemeColorRow(L"ThemeRowKnobCapEnd", m_theme.KnobCapEndColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.KnobCapEndColor, true); });
            });

        AddThemeSliderRow(L"ThemeRowKnobCapSize", L"ThemeUnitPercent", 5, 100, m_theme.KnobCapSizePercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.KnobCapSizePercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSwitchRow(L"ThemeRowPointerOnCap", m_theme.PointerOnCap,
            [edit](bool on)
            {
                edit([on](glass::Theme& theme) { theme.PointerOnCap = on; });
            });

        AddThemeSliderRow(L"ThemeRowKnobTicks", nullptr, 0, 32, m_theme.KnobTickCount,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.KnobTickCount = static_cast<int32_t>(std::lround(value)); });
            });

        // ---- switches ----

        AddThemeGroupHeading(L"ThemeGroupSwitches", false);

        AddThemeSliderRow(L"ThemeRowOnLift", L"ThemeUnitPercent", -100, 100, m_theme.OnLiftPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.OnLiftPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeNote(L"ThemeOnLiftNote");

        AddThemeColorRow(L"ThemeRowLampColor", m_theme.LampColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.LampColor, true); });
            });

        AddThemeComboRow(L"ThemeRowLampShape",
            { L"ThemeLampBar", L"ThemeLampDot" },
            static_cast<int32_t>(m_theme.LampShape),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme)
                    { theme.LampShape = static_cast<glass::LampStyle>(index); });
            });

        AddThemeSwitchRow(L"ThemeRowNeutralCaps", m_theme.NeutralCaps,
            [edit](bool on)
            {
                edit([on](glass::Theme& theme) { theme.NeutralCaps = on; });
            });

        AddThemeNote(L"ThemeNeutralCapsNote");

        // ---- sections ----

        AddThemeGroupHeading(L"ThemeGroupSections", false);

        AddThemeComboRow(L"ThemeRowPanelFill",
            { L"ThemePanelFillPlate", L"ThemePanelFillColor", L"ThemePanelFillNone" },
            static_cast<int32_t>(m_theme.PanelFill),
            [edit](int32_t index)
            {
                edit([index](glass::Theme& theme)
                    { theme.PanelFill = static_cast<glass::PanelFillStyle>(index); });
            });

        AddThemeColorRow(L"ThemeRowPanelColor", m_theme.PanelColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.PanelColor, true); });
            });

        AddThemeColorRow(L"ThemeRowPanelEnd", m_theme.PanelEndColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.PanelEndColor, true); });
            });

        AddThemeColorRow(L"ThemeRowPanelOutline", m_theme.PanelOutlineColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.PanelOutlineColor, true); });
            });

        // Minus one follows the plate's own lift, which is what every section did before a
        // theme arrived whose sections are printed rather than raised.
        AddThemeSliderRow(L"ThemeRowPanelElevation", L"ThemeUnitPercentOrFollow", -1, 100, m_theme.PanelElevation,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.PanelElevation = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeColorRow(L"ThemeRowInsetPanel", m_theme.InsetPanelColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.InsetPanelColor, true); });
            });

        AddThemeColorRow(L"ThemeRowInsetPanelEnd", m_theme.InsetPanelEndColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.InsetPanelEndColor, true); });
            });

        AddThemeNote(L"ThemeInsetPanelNote");

        AddThemeColorRow(L"ThemeRowSectionInk", m_theme.SectionInkColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.SectionInkColor, true); });
            });

        AddThemeNote(L"ThemeSectionInkNote");

        AddThemeColorRow(L"ThemeRowRuleColor", m_theme.RuleColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.RuleColor, true); });
            });

        AddThemeSwitchRow(L"ThemeRowRuleFades", m_theme.RuleFades,
            [edit](bool on)
            {
                edit([on](glass::Theme& theme) { theme.RuleFades = on; });
            });

        // ---- the piano keyboard ----

        AddThemeGroupHeading(L"ThemeGroupKeyboard", false);

        AddThemeColorRow(L"ThemeRowKeyWhite", m_theme.KeyWhiteColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.KeyWhiteColor, true); });
            });

        AddThemeColorRow(L"ThemeRowKeyBlack", m_theme.KeyBlackColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.KeyBlackColor, true); });
            });

        // ---- the light ----

        AddThemeGroupHeading(L"ThemeGroupLight", false);

        AddThemeColorRow(L"ThemeRowLightColor", m_theme.BloomColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.BloomColor, true); });
            });

        AddThemeNote(L"ThemeLightColorNote");

        AddThemeSliderRow(L"ThemeRowRestingGlow", L"ThemeUnitPercent", 0, 100, m_theme.RestingGlowPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.RestingGlowPercent = static_cast<int32_t>(std::lround(value)); });
            });

        if (m_theme.RestingGlowPercent > 0)
        {
            AddThemeNote(L"ThemeRestingGlowIsStructure");
        }

        AddThemeSliderRow(L"ThemeRowSwitchRestingGlow", L"ThemeUnitPercentOrFollow", -1, 100,
            m_theme.SwitchRestingGlowPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.SwitchRestingGlowPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowStaysLit", L"ThemeUnitMilliseconds", 0, 2000, m_theme.PersistenceMilliseconds,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.PersistenceMilliseconds = static_cast<int32_t>(std::lround(value / 10.0) * 10); });
            });

        // ---- the glass over the whole page ----

        AddThemeGroupHeading(L"ThemeGroupOverlay", false);
        AddThemeNote(L"ThemeOverlayNote");

        AddThemeSliderRow(L"ThemeRowScanLines", L"ThemeUnitPixels", 0, 12, m_theme.Overlay.ScanLinePitch,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.Overlay.ScanLinePitch = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeSliderRow(L"ThemeRowScanStrength", L"ThemeUnitPercent", 0, 100, m_theme.Overlay.ScanLineStrength,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.Overlay.ScanLineStrength = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeColorRow(L"ThemeRowScanColor", m_theme.Overlay.ScanLineColor, false,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.Overlay.ScanLineColor, false); });
            });

        AddThemeSliderRow(L"ThemeRowVignette", L"ThemeUnitPercent", 0, 100, m_theme.Overlay.VignettePercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.Overlay.VignettePercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeColorRow(L"ThemeRowVignetteColor", m_theme.Overlay.VignetteColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.Overlay.VignetteColor, true); });
            });

        AddThemeSliderRow(L"ThemeRowFaceplate", L"ThemeUnitPercent", 0, 100, m_theme.Overlay.FaceplateSheenPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.Overlay.FaceplateSheenPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeColorRow(L"ThemeRowFaceplateColor", m_theme.Overlay.FaceplateSheenColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.Overlay.FaceplateSheenColor, true); });
            });

        AddThemeSliderRow(L"ThemeRowGrain", L"ThemeUnitPercent", 0, 100, m_theme.Overlay.GrainPercent,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.Overlay.GrainPercent = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeColorRow(L"ThemeRowGrainColor", m_theme.Overlay.GrainColor, true,
            [edit, color](std::wstring const& code)
            {
                edit([code, color](glass::Theme& theme) { color(code, theme.Overlay.GrainColor, true); });
            });

        AddThemeNote(L"ThemeGrainNote");

        AddThemeSliderRow(L"ThemeRowGrainStreak", nullptr, 1, 64, m_theme.Overlay.GrainStreak,
            [edit](double value)
            {
                edit([value](glass::Theme& theme)
                    { theme.Overlay.GrainStreak = static_cast<int32_t>(std::lround(value)); });
            });

        AddThemeNote(L"ThemeGrainStreakNote");

        // ---- the meter's three zones ----

        AddThemeGroupHeading(L"ThemeGroupMeter", false);
        AddThemeNote(L"ThemeMeterNote");

        std::vector<wchar_t const*> const slotNames
        {
            L"ThemeSlotOne", L"ThemeSlotTwo", L"ThemeSlotThree",
            L"ThemeSlotFour", L"ThemeSlotFive", L"ThemeSlotSix",
        };

        wchar_t const* const zoneKeys[]
        {
            L"ThemeRowMeterSignal", L"ThemeRowMeterWarning", L"ThemeRowMeterTooLoud",
        };

        for (int32_t zone = 0; zone < glass::MeterZoneCount; ++zone)
        {
            AddThemeComboRow(zoneKeys[zone], slotNames,
                m_theme.MeterSlots[static_cast<size_t>(zone)],
                [edit, zone](int32_t index)
                {
                    edit([zone, index](glass::Theme& theme)
                        { theme.MeterSlots[static_cast<size_t>(zone)] = index; });
                });
        }

        m_updatingSettings = false;
    }

    // Whether this theme's plate can separate itself from its deck on its own. When it cannot,
    // the elevation shadow is the structure and the editor says so rather than letting somebody
    // flatten it and wonder where the layout went.
    bool EditorWindow::PlateNeedsItsShadow() const
    {
        if (m_theme.PlateColor.A == 0)
        {
            return false;
        }

        // Measured against the darker end, which is the worst case a control sits on.
        return glass::ContrastRatio(m_theme.PlateColor, m_theme.Deck.GradientEndColor) < 1.5;
    }
}
