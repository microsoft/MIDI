// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppearanceFlyout.h"

namespace wux = ::winrt::Microsoft::UI::Xaml;
namespace wuxc = ::winrt::Microsoft::UI::Xaml::Controls;

namespace midiapp
{
    namespace
    {
        wuxc::ComboBox MakePicker(winrt::hstring const& header, std::vector<winrt::hstring> const& items, int32_t selected)
        {
            wuxc::ComboBox box{};

            box.Header(winrt::box_value(header));
            box.Width(260.0);
            box.HorizontalAlignment(wux::HorizontalAlignment::Left);

            auto source = winrt::single_threaded_vector<winrt::Windows::Foundation::IInspectable>();

            for (auto const& item : items)
            {
                source.Append(winrt::box_value(item));
            }

            box.ItemsSource(source);
            box.SelectedIndex(selected);

            return box;
        }

        winrt::Windows::UI::Color ColorFromArgb(uint32_t argb) noexcept
        {
            winrt::Windows::UI::Color color{};

            color.A = static_cast<uint8_t>((argb >> 24) & 0xFF);
            color.R = static_cast<uint8_t>((argb >> 16) & 0xFF);
            color.G = static_cast<uint8_t>((argb >> 8) & 0xFF);
            color.B = static_cast<uint8_t>(argb & 0xFF);

            return color;
        }
    }

    void ShowAppearanceFlyout(
        wux::FrameworkElement const& anchor,
        MidiAppSettings& settings,
        AppearanceStrings const& strings,
        std::function<void()> const& onChanged,
        wux::UIElement const& extraContent) noexcept
    {
        try
        {
            if (anchor == nullptr)
            {
                return;
            }

            auto* const settingsPtr = &settings;

            wuxc::StackPanel panel{};
            panel.Spacing(16.0);
            panel.Width(560.0);
            panel.Margin(wux::Thickness{ 4, 0, 16, 0 });

            wuxc::TextBlock title{};
            title.Text(strings.Title);
            title.Style(wux::Application::Current().Resources()
                .Lookup(winrt::box_value(L"SubtitleTextBlockStyle")).as<wux::Style>());
            panel.Children().Append(title);

            auto themeBox = MakePicker(strings.ThemeLabel,
                { strings.ThemeSystem, strings.ThemeLight, strings.ThemeDark },
                static_cast<int32_t>(settings.Theme()));

            auto backdropBox = MakePicker(strings.BackdropLabel,
                { strings.BackdropSolid, strings.BackdropMica, strings.BackdropAcrylic },
                static_cast<int32_t>(settings.Backdrop()));

            wuxc::CheckBox customColor{};
            customColor.Content(winrt::box_value(strings.CustomColorCheckBox));
            customColor.IsChecked(settings.UseCustomBackgroundColor());

            wuxc::ColorPicker picker{};
            picker.IsAlphaEnabled(false);
            picker.IsColorSliderVisible(true);
            picker.IsColorChannelTextInputVisible(true);
            picker.IsHexInputVisible(true);
            picker.ColorSpectrumShape(wuxc::ColorSpectrumShape::Box);
            picker.Orientation(wuxc::Orientation::Horizontal);
            picker.HorizontalAlignment(wux::HorizontalAlignment::Left);
            picker.Color(ColorFromArgb(settings.BackgroundColorArgb()));
            picker.IsEnabled(settings.UseCustomBackgroundColor());
            wux::Automation::AutomationProperties::SetName(picker, strings.ColorPickerName);

            // the stock Horizontal visual state pins the picker to a 312px minimum, ~56px more
            // than it draws, and the value comes from generic.xaml via StaticResource so it
            // cannot be overridden; take the reserved space back here
            picker.Margin(wux::Thickness{ 0, 0, 0, -56 });

            themeBox.SelectionChanged([settingsPtr, onChanged](auto const& sender, auto&&)
                {
                    auto const index = sender.template as<wuxc::ComboBox>().SelectedIndex();

                    if (index >= 0)
                    {
                        settingsPtr->Theme(static_cast<AppTheme>(index));
                        onChanged();
                    }
                });

            backdropBox.SelectionChanged([settingsPtr, onChanged](auto const& sender, auto&&)
                {
                    auto const index = sender.template as<wuxc::ComboBox>().SelectedIndex();

                    if (index >= 0)
                    {
                        settingsPtr->Backdrop(static_cast<WindowBackdrop>(index));
                        onChanged();
                    }
                });

            auto const onCustomColorChanged = [settingsPtr, picker, customColor, onChanged](auto&&, auto&&)
                {
                    auto const checked = customColor.IsChecked();
                    auto const isOn = checked != nullptr && checked.Value();

                    settingsPtr->UseCustomBackgroundColor(isOn);
                    picker.IsEnabled(isOn);
                    onChanged();
                };

            customColor.Checked(onCustomColorChanged);
            customColor.Unchecked(onCustomColorChanged);

            picker.ColorChanged([settingsPtr, onChanged](auto&&, wuxc::ColorChangedEventArgs const& args)
                {
                    auto const color = args.NewColor();

                    auto const argb =
                        (static_cast<uint32_t>(0xFF) << 24) |
                        (static_cast<uint32_t>(color.R) << 16) |
                        (static_cast<uint32_t>(color.G) << 8) |
                        static_cast<uint32_t>(color.B);

                    settingsPtr->BackgroundColorArgb(argb);
                    onChanged();
                });

            panel.Children().Append(themeBox);
            panel.Children().Append(backdropBox);
            panel.Children().Append(customColor);
            panel.Children().Append(picker);

            // safe to follow the picker: its negative bottom margin only gives back space the
            // picker measures but never draws in
            if (extraContent != nullptr)
            {
                panel.Children().Append(extraContent);
            }

            wuxc::ScrollViewer scroller{};
            scroller.VerticalScrollBarVisibility(wuxc::ScrollBarVisibility::Auto);
            scroller.MaxHeight(640.0);
            scroller.Content(panel);

            wuxc::Flyout flyout{};

            // BasedOn matters: an explicit presenter style replaces the implicit one outright,
            // which loses the rounded corners, border and padding
            if (auto const baseStyle = wux::Application::Current().Resources()
                .TryLookup(winrt::box_value(L"DefaultFlyoutPresenterStyle")))
            {
                winrt::Windows::UI::Xaml::Interop::TypeName const presenterType
                {
                    winrt::hstring{ L"Microsoft.UI.Xaml.Controls.FlyoutPresenter" },
                    winrt::Windows::UI::Xaml::Interop::TypeKind::Metadata
                };

                wux::Style presenterStyle{ presenterType };

                presenterStyle.BasedOn(baseStyle.as<wux::Style>());
                presenterStyle.Setters().Append(
                    wux::Setter{ wux::FrameworkElement::MaxWidthProperty(), winrt::box_value(680.0) });
                presenterStyle.Setters().Append(
                    wux::Setter{ wux::FrameworkElement::MaxHeightProperty(), winrt::box_value(760.0) });

                flyout.FlyoutPresenterStyle(presenterStyle);
            }

            flyout.Content(scroller);
            flyout.Placement(wuxc::Primitives::FlyoutPlacementMode::TopEdgeAlignedLeft);
            flyout.ShowAt(anchor);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }
}
