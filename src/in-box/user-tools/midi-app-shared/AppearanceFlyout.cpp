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

        // the pickers have no alpha channel, so every color saved from one is opaque
        uint32_t ArgbFromColor(_In_ winrt::Windows::UI::Color const& color) noexcept
        {
            return
                (static_cast<uint32_t>(0xFF) << 24) |
                (static_cast<uint32_t>(color.R) << 16) |
                (static_cast<uint32_t>(color.G) << 8) |
                static_cast<uint32_t>(color.B);
        }

        struct ColorSection
        {
            wuxc::CheckBox CheckBox{ nullptr };
            wuxc::ColorPicker Picker{ nullptr };
        };

        ColorSection MakeColorSection(
            _In_ winrt::hstring const& checkBoxLabel,
            _In_ winrt::hstring const& pickerName,
            _In_ bool useCustomColor,
            _In_ uint32_t colorArgb)
        {
            ColorSection section{ wuxc::CheckBox{}, wuxc::ColorPicker{} };

            section.CheckBox.Content(winrt::box_value(checkBoxLabel));
            section.CheckBox.IsChecked(useCustomColor);

            auto const& picker = section.Picker;

            picker.IsAlphaEnabled(false);
            picker.IsColorSliderVisible(true);
            picker.IsColorChannelTextInputVisible(true);
            picker.IsHexInputVisible(true);
            picker.ColorSpectrumShape(wuxc::ColorSpectrumShape::Box);
            picker.Orientation(wuxc::Orientation::Horizontal);
            picker.HorizontalAlignment(wux::HorizontalAlignment::Left);
            picker.Color(ColorFromArgb(colorArgb));
            picker.IsEnabled(useCustomColor);
            wux::Automation::AutomationProperties::SetName(picker, pickerName);

            // the stock Horizontal visual state pins the picker to a 312px minimum, ~56px more
            // than it draws, and the value comes from generic.xaml via StaticResource so it
            // cannot be overridden; take the reserved space back here
            picker.Margin(wux::Thickness{ 0, 0, 0, -56 });

            return section;
        }

        // One page per color, the window's first, with a row of tabs that shows one at a time.
        wux::UIElement MakeColorTabs(
            _In_ AppearanceColorTabs const& colorTabs,
            _In_ ColorSection const& windowColor)
        {
            wuxc::SelectorBar tabs{};
            wux::Automation::AutomationProperties::SetName(tabs, colorTabs.TabsName);

            wuxc::Grid pageHost{};
            std::vector<wux::UIElement> pages{};

            auto const addPage = [&tabs, &pageHost, &pages](winrt::hstring const& label, ColorSection const& section)
                {
                    wuxc::SelectorBarItem tab{};
                    tab.Text(label);
                    wux::Automation::AutomationProperties::SetName(tab, label);
                    tabs.Items().Append(tab);

                    wuxc::StackPanel page{};
                    page.Spacing(16.0);
                    page.Children().Append(section.CheckBox);
                    page.Children().Append(section.Picker);
                    page.Visibility(pages.empty() ? wux::Visibility::Visible : wux::Visibility::Collapsed);

                    pageHost.Children().Append(page);
                    pages.push_back(page);
                };

            addPage(colorTabs.WindowTabLabel, windowColor);

            for (auto const& choice : colorTabs.Colors)
            {
                auto const section = MakeColorSection(
                    choice.CustomColorCheckBox, choice.ColorPickerName, choice.UseCustomColor, choice.ColorArgb);

                // each control reports both values, so each has to see what the other last set
                auto const state = std::make_shared<AppearanceColorChoice>(choice);

                auto const onCustomColorChanged = [state, picker = section.Picker](
                    winrt::Windows::Foundation::IInspectable const& sender, wux::RoutedEventArgs const&)
                    {
                        auto const checked = sender.as<wuxc::CheckBox>().IsChecked();

                        state->UseCustomColor = checked != nullptr && checked.Value();
                        picker.IsEnabled(state->UseCustomColor);

                        if (state->Changed)
                        {
                            state->Changed(state->UseCustomColor, state->ColorArgb);
                        }
                    };

                section.CheckBox.Checked(onCustomColorChanged);
                section.CheckBox.Unchecked(onCustomColorChanged);

                section.Picker.ColorChanged([state](auto&&, wuxc::ColorChangedEventArgs const& args)
                    {
                        state->ColorArgb = ArgbFromColor(args.NewColor());

                        if (state->Changed)
                        {
                            state->Changed(state->UseCustomColor, state->ColorArgb);
                        }
                    });

                addPage(choice.TabLabel, section);
            }

            tabs.SelectedItem(tabs.Items().GetAt(0));

            tabs.SelectionChanged([pages](wuxc::SelectorBar const& sender, wuxc::SelectorBarSelectionChangedEventArgs const&)
                {
                    auto const selectedTab = sender.SelectedItem();
                    uint32_t selected{ 0 };

                    if (selectedTab == nullptr || !sender.Items().IndexOf(selectedTab, selected))
                    {
                        return;
                    }

                    for (size_t i = 0; i < pages.size(); i++)
                    {
                        pages[i].Visibility(i == selected ? wux::Visibility::Visible : wux::Visibility::Collapsed);
                    }
                });

            wuxc::StackPanel host{};
            host.Spacing(8.0);
            host.Children().Append(tabs);
            host.Children().Append(pageHost);

            return host;
        }
    }

    _Use_decl_annotations_
    void ShowAppearanceFlyout(
        wux::FrameworkElement const& anchor,
        MidiAppSettings& settings,
        AppearanceStrings const& strings,
        std::function<void()> const& onChanged,
        wux::UIElement const& extraContent,
        wux::UIElement const& topContent,
        AppearanceColorTabs const& colorTabs) noexcept
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

            if (topContent != nullptr)
            {
                panel.Children().Append(topContent);
            }

            auto themeBox = MakePicker(strings.ThemeLabel,
                { strings.ThemeSystem, strings.ThemeLight, strings.ThemeDark },
                static_cast<int32_t>(settings.Theme()));

            auto backdropBox = MakePicker(strings.BackdropLabel,
                { strings.BackdropSolid, strings.BackdropMica, strings.BackdropAcrylic },
                static_cast<int32_t>(settings.Backdrop()));

            auto const windowColor = MakeColorSection(
                strings.CustomColorCheckBox,
                strings.ColorPickerName,
                settings.UseCustomBackgroundColor(),
                settings.BackgroundColorArgb());

            auto const customColor = windowColor.CheckBox;
            auto const picker = windowColor.Picker;

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
                    settingsPtr->BackgroundColorArgb(ArgbFromColor(args.NewColor()));
                    onChanged();
                });

            panel.Children().Append(themeBox);
            panel.Children().Append(backdropBox);

            if (colorTabs.Colors.empty())
            {
                panel.Children().Append(customColor);
                panel.Children().Append(picker);
            }
            else
            {
                panel.Children().Append(MakeColorTabs(colorTabs, windowColor));
            }

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
