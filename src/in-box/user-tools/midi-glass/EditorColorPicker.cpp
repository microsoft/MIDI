// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The button that sits beside every color code in the editor.
//
// Typing #16191F into a text box is fine once you know what you want. Nobody knows what they
// want the second key color to be until they can see it, so every place the editor asks for a
// color gets the same flyout: the theme's own six slots first, because a layout that stays in
// its theme keeps working when the theme is swapped, and a full picker underneath for the times
// it has to be something else.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "ThemeStore.h"

namespace resources = ::midiglass::resources;
namespace shapes = ::winrt::Microsoft::UI::Xaml::Shapes;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr double SwatchSize = 26.0;

        winrt::Windows::UI::Color ToWindowsColor(_In_ glass::ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }

        std::wstring ToColorCode(_In_ winrt::Windows::UI::Color const& color)
        {
            return std::format(L"#{:02X}{:02X}{:02X}", color.R, color.G, color.B);
        }

        // The swatch on the button, and the swatches in the flyout. A Rectangle rather than a
        // Border: a Border's corner radius is aliased at fractional scaling.
        shapes::Rectangle MakeSwatch(_In_ double size)
        {
            shapes::Rectangle swatch{};

            swatch.Width(size);
            swatch.Height(size);
            swatch.RadiusX(4);
            swatch.RadiusY(4);
            swatch.UseLayoutRounding(false);
            swatch.StrokeThickness(1);
            swatch.Stroke(xaml::Application::Current().Resources()
                .Lookup(box_value(L"ControlStrokeColorDefaultBrush")).as<media::Brush>());

            return swatch;
        }
    }

    // Paints the button's own swatch from whatever the box says right now. A blank box is drawn
    // as a hatch rather than as black, because "no color" and "black" are different answers and
    // the tint box takes both.
    _Use_decl_annotations_
    void EditorWindow::RefreshColorButton(
        xaml::Controls::Button const& button,
        std::wstring const& code)
    {
        auto const swatch = button.Content().try_as<shapes::Rectangle>();

        if (swatch == nullptr)
        {
            return;
        }

        glass::ThemeColor parsed{};

        if (!code.empty() && glass::TryParseColor(code, parsed))
        {
            swatch.Fill(media::SolidColorBrush(ToWindowsColor(parsed)));
            button.SetValue(xaml::Automation::AutomationProperties::HelpTextProperty(),
                box_value(winrt::hstring{ code }));

            return;
        }

        swatch.Fill(xaml::Application::Current().Resources()
            .Lookup(box_value(L"ControlAltFillColorSecondaryBrush")).as<media::Brush>());

        button.SetValue(xaml::Automation::AutomationProperties::HelpTextProperty(),
            box_value(resources::GetString(L"ColorPickerNone")));
    }

    // Wires one button to one text box. The box stays: a color code is still the fastest way to
    // paste the exact color out of a brand guide, and it is the only thing a screen reader can
    // read back.
    _Use_decl_annotations_
    void EditorWindow::AttachColorPicker(
        xaml::Controls::Button const& button,
        xaml::Controls::TextBox const& box,
        bool allowEmpty,
        std::function<void()> const& changed)
    {
        try
        {
            button.Content(MakeSwatch(18.0));
            button.Padding({ 5, 5, 5, 5 });
            button.MinWidth(0);

            RefreshColorButton(button, std::wstring{ box.Text() });

            // Typing in the box repaints the swatch. The box's own committed-value handler is
            // what tells the document; this only keeps the two in step while it is being typed.
            box.TextChanged([weak = get_weak(), button, box](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->RefreshColorButton(button, std::wstring{ box.Text() });
                    }
                });

            auto const apply = [weak = get_weak(), button, box, changed](std::wstring const& code)
                {
                    box.Text(winrt::hstring{ code });

                    if (auto strong = weak.get())
                    {
                        strong->RefreshColorButton(button, code);
                    }

                    if (changed)
                    {
                        changed();
                    }
                };

            button.Click([weak = get_weak(), button, box, allowEmpty, apply](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    strong->ShowColorFlyout(button, std::wstring{ box.Text() }, allowEmpty, apply);
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to attach a color picker.")
    }

    _Use_decl_annotations_
    void EditorWindow::ShowColorFlyout(
        xaml::FrameworkElement const& anchor,
        std::wstring const& current,
        bool allowEmpty,
        std::function<void(std::wstring const&)> const& apply)
    {
        try
        {
            controls::Flyout flyout{};

            controls::StackPanel root{};
            root.Spacing(8);
            root.MinWidth(300);

            // ---- the theme's own slots ----

            controls::TextBlock heading{};
            heading.Text(resources::GetString(L"ColorPickerThemeHeading"));
            heading.FontSize(11);
            heading.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());

            controls::StackPanel slots{};
            slots.Orientation(controls::Orientation::Horizontal);
            slots.Spacing(6);

            for (int32_t slot = 0; slot < glass::HueSlotCount; ++slot)
            {
                auto const color = m_theme.HueSlots[static_cast<size_t>(slot)];
                auto const code = ToColorCode(ToWindowsColor(color));

                controls::Button swatchButton{};

                auto swatch = MakeSwatch(SwatchSize);
                swatch.Fill(media::SolidColorBrush(ToWindowsColor(color)));

                swatchButton.Content(swatch);
                swatchButton.Padding({ 3, 3, 3, 3 });
                swatchButton.MinWidth(0);

                auto const name = resources::FormatString(
                    L"ColorPickerSlotFormat", std::to_wstring(slot + 1), code);

                xaml::Automation::AutomationProperties::SetName(swatchButton, name);
                controls::ToolTipService::SetToolTip(swatchButton, box_value(name));

                swatchButton.Click([flyout, apply, code](auto&&, auto&&)
                    {
                        apply(code);
                        flyout.Hide();
                    });

                slots.Children().Append(swatchButton);
            }

            // ---- anything else ----

            controls::ColorPicker picker{};

            picker.IsAlphaEnabled(false);
            picker.IsColorSliderVisible(true);
            picker.IsHexInputVisible(true);
            picker.IsColorChannelTextInputVisible(false);
            picker.ColorSpectrumShape(controls::ColorSpectrumShape::Box);
            picker.MaxHeight(280);

            glass::ThemeColor parsed{};

            if (!current.empty() && glass::TryParseColor(current, parsed))
            {
                picker.Color(ToWindowsColor(parsed));
            }

            controls::StackPanel buttons{};
            buttons.Orientation(controls::Orientation::Horizontal);
            buttons.Spacing(8);

            controls::Button use{};
            use.Content(box_value(resources::GetString(L"ColorPickerUse")));
            use.Style(xaml::Application::Current().Resources()
                .Lookup(box_value(L"AccentButtonStyle")).as<xaml::Style>());

            use.Click([flyout, apply, picker](auto&&, auto&&)
                {
                    apply(ToColorCode(picker.Color()));
                    flyout.Hide();
                });

            buttons.Children().Append(use);

            // Only where blank is a real answer. On a keyboard's white keys it is: blank means
            // the theme decides, which is what almost every layout should stay on.
            if (allowEmpty)
            {
                controls::Button clear{};
                clear.Content(box_value(resources::GetString(L"ColorPickerClear")));

                clear.Click([flyout, apply](auto&&, auto&&)
                    {
                        apply(std::wstring{});
                        flyout.Hide();
                    });

                buttons.Children().Append(clear);
            }

            root.Children().Append(heading);
            root.Children().Append(slots);
            root.Children().Append(picker);
            root.Children().Append(buttons);

            flyout.Content(root);
            flyout.ShowAt(anchor);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the color picker.")
    }
}
