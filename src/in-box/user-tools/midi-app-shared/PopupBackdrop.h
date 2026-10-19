// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Not every tool's pch pulls in these, and this header uses them directly.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

namespace midiapp
{
    // ========================================================================================
    // Pop-ups that match the context menus
    //
    // WinUI gives a context menu (a MenuFlyout) the acrylic system backdrop and a see-through
    // background: its presenter style sets SystemBackdrop to MenuFlyoutSystemBackdrop and
    // Background to MenuFlyoutPresenterBackground. So a menu takes on the window's tint, and
    // the color the customer picked in Appearance. A Flyout, a ComboBox's list, an
    // AutoSuggestBox's suggestions, and a NumberBox's pop-up spin buttons don't get it: they keep
    // their own gray fill, which looks out of place beside a menu.
    //
    // UseMenuBackdrop(flyout) gives a Flyout the menu's backdrop and background. It also makes
    // the flyout a window of its own, as a menu is, because a system backdrop needs one.
    //
    // The other three can't have the backdrop. Each is a Popup in its control's template, and
    // WinUI draws the backdrop of a pop-up that's in the visual tree offset by where the
    // pop-up's parent is in the window. That puts it outside the pop-up's own window and leaves
    // the pop-up see-through (microsoft-ui-xaml issue 10087). So they're filled with the color
    // the menus show over the window instead, worked out the way WinUI's acrylic works it out:
    // the window's color brought to the luminosity of the acrylic's tint, with a little of the
    // tint on top. Over a plain window that's the menus' own color, without the acrylic's
    // grain. The window chrome passes in the window's color each time it applies it, and the
    // fill follows. In high contrast these pop-ups keep WinUI's colors.
    //
    // UseMenuBackdropForLists turns this on. It puts the fill in the app's theme resources,
    // where a control made later finds it, and watches focus for the whole thread to fill the
    // pop-ups of controls made before that: a ComboBox, a NumberBox, or the text box in an
    // AutoSuggestBox or an editable ComboBox takes focus before its pop-up opens, and a
    // ComboBox's items take it while its list is open. One call covers every one of these
    // controls, in flyouts and dialogs too.
    //
    // A tool that calls UseMenuBackdropForLists gets the backdrop on the shared Appearance
    // flyout as well.
    // ========================================================================================

    namespace details
    {
        inline thread_local bool MenuBackdropsOn{ false };

        // The window's color as the window chrome last applied it, which the pop-ups' fill is
        // worked out from.
        inline thread_local bool WindowColorIsCustom{ false };
        inline thread_local uint32_t WindowColorArgb{ 0 };

        // The theme resources WinUI fills each kind of template pop-up with.
        inline constexpr wchar_t ComboBoxFillKey[]{ L"ComboBoxDropDownBackground" };
        inline constexpr wchar_t AutoSuggestBoxFillKey[]{ L"AutoSuggestBoxSuggestionsListBackground" };
        inline constexpr wchar_t NumberBoxFillKey[]{ L"NumberBoxPopupBackground" };

        inline winrt::Windows::Foundation::IInspectable MenuResource(_In_ wchar_t const* key)
        {
            return winrt::Microsoft::UI::Xaml::Application::Current().Resources().TryLookup(winrt::box_value(key));
        }

        // A brush in one of the app's theme dictionaries: made the first time, recolored after.
        inline void SetThemeBrush(_In_ wchar_t const* theme, _In_ wchar_t const* key, winrt::Windows::UI::Color color)
        {
            namespace xaml = winrt::Microsoft::UI::Xaml;

            auto const themes = xaml::Application::Current().Resources().ThemeDictionaries();
            xaml::ResourceDictionary dictionary{ nullptr };

            if (auto const found = themes.TryLookup(winrt::box_value(theme)))
            {
                dictionary = found.as<xaml::ResourceDictionary>();
            }
            else
            {
                dictionary = xaml::ResourceDictionary{};
                themes.Insert(winrt::box_value(theme), dictionary);
            }

            if (auto const brush = dictionary.TryLookup(winrt::box_value(key)).try_as<xaml::Media::SolidColorBrush>())
            {
                brush.Color(color);
            }
            else
            {
                dictionary.Insert(winrt::box_value(key), xaml::Media::SolidColorBrush{ color });
            }
        }

        // The brush SetThemeBrush put in one of the app's theme dictionaries, or null.
        inline winrt::Microsoft::UI::Xaml::Media::SolidColorBrush ThemeBrush(_In_ wchar_t const* theme, _In_ wchar_t const* key)
        {
            namespace xaml = winrt::Microsoft::UI::Xaml;

            if (auto const dictionary = xaml::Application::Current().Resources().ThemeDictionaries().TryLookup(winrt::box_value(theme)).try_as<xaml::ResourceDictionary>())
            {
                return dictionary.TryLookup(winrt::box_value(key)).try_as<xaml::Media::SolidColorBrush>();
            }

            return nullptr;
        }

        // The luminosity WinUI's acrylic blends by (the W3C compositing spec's), from 0 to 255.
        inline double BlendLuminosity(double red, double green, double blue) noexcept
        {
            return 0.3 * red + 0.59 * green + 0.11 * blue;
        }

        // What the context menus' acrylic shows over a window of one color. It's what WinUI's
        // AcrylicBrush makes of it with the menus' settings (AcrylicBackgroundFillColorDefault):
        // the window's color at the luminosity of the tint, mixed in by the luminosity opacity,
        // then a gray of the result's own luminosity, mixed in by the tint opacity. Menus
        // measured over two window colors come within 3 of this in each channel.
        inline winrt::Windows::UI::Color MenuColorOver(winrt::Windows::UI::Color window, bool dark) noexcept
        {
            double const tint = dark ? 0x2C : 0xFC;
            double const luminosityOpacity = dark ? 0.96 : 0.85;
            double const tintOpacity = dark ? 0.15 : 0.0;

            double const red = window.R;
            double const green = window.G;
            double const blue = window.B;

            // Moved to the tint's luminosity, then drawn in toward gray as far as it takes to stay
            // in range, as the compositing spec's luminosity blend does.
            double const shift = tint - BlendLuminosity(red, green, blue);
            double r = red + shift;
            double g = green + shift;
            double b = blue + shift;

            double const lowest = (std::min)({ r, g, b });
            double const highest = (std::max)({ r, g, b });

            if (lowest < 0.0)
            {
                double const scale = tint / (tint - lowest);
                r = tint + (r - tint) * scale;
                g = tint + (g - tint) * scale;
                b = tint + (b - tint) * scale;
            }

            if (highest > 255.0)
            {
                double const scale = (255.0 - tint) / (highest - tint);
                r = tint + (r - tint) * scale;
                g = tint + (g - tint) * scale;
                b = tint + (b - tint) * scale;
            }

            r = red + (r - red) * luminosityOpacity;
            g = green + (g - green) * luminosityOpacity;
            b = blue + (b - blue) * luminosityOpacity;

            double const gray = BlendLuminosity(r, g, b);
            r += (gray - r) * tintOpacity;
            g += (gray - g) * tintOpacity;
            b += (gray - b) * tintOpacity;

            auto const channel = [](double value) noexcept
            {
                return static_cast<uint8_t>(std::lround((std::clamp)(value, 0.0, 255.0)));
            };

            return winrt::Windows::UI::Color{ 255, channel(r), channel(g), channel(b) };
        }

        // The window's color in one theme: the picked color over the theme's own background
        // (WinUI's SolidBackgroundFillColorBase), or that background alone.
        inline winrt::Windows::UI::Color WindowColor(bool dark) noexcept
        {
            winrt::Windows::UI::Color const plain = dark
                ? winrt::Windows::UI::Color{ 255, 0x20, 0x20, 0x20 }
                : winrt::Windows::UI::Color{ 255, 0xF3, 0xF3, 0xF3 };

            if (!WindowColorIsCustom)
            {
                return plain;
            }

            double const alpha = ((WindowColorArgb >> 24) & 0xFF) / 255.0;

            auto const over = [alpha](uint32_t picked, uint8_t below) noexcept
            {
                return static_cast<uint8_t>(std::lround(below + (static_cast<double>(picked & 0xFF) - below) * alpha));
            };

            return winrt::Windows::UI::Color{ 255, over(WindowColorArgb >> 16, plain.R), over(WindowColorArgb >> 8, plain.G), over(WindowColorArgb, plain.B) };
        }

        // The pop-ups' fill in the app's theme dictionaries for the dark and light themes, for
        // the window's color now. Their brushes are recolored in place, so a pop-up already
        // filled with one changes with it. High contrast keeps WinUI's own.
        inline void ApplyPopupFill() noexcept
        {
            try
            {
                auto const dark = MenuColorOver(WindowColor(true), true);
                auto const light = MenuColorOver(WindowColor(false), false);

                for (auto const key : { ComboBoxFillKey, AutoSuggestBoxFillKey, NumberBoxFillKey })
                {
                    SetThemeBrush(L"Default", key, dark);
                    SetThemeBrush(L"Light", key, light);
                }
            }
            catch (...)
            {
            }
        }

        // The Popup in a control's template, a child or two below the control.
        inline winrt::Microsoft::UI::Xaml::Controls::Primitives::Popup TemplatePopup(_In_ winrt::Microsoft::UI::Xaml::DependencyObject const& control)
        {
            namespace xaml = winrt::Microsoft::UI::Xaml;

            std::vector<xaml::DependencyObject> level{ control };

            for (int depth = 0; depth < 3 && !level.empty(); ++depth)
            {
                std::vector<xaml::DependencyObject> next{};

                for (auto const& parent : level)
                {
                    auto const count = xaml::Media::VisualTreeHelper::GetChildrenCount(parent);

                    for (int32_t i = 0; i < count; ++i)
                    {
                        auto const child = xaml::Media::VisualTreeHelper::GetChild(parent, i);

                        if (auto const popup = child.try_as<xaml::Controls::Primitives::Popup>())
                        {
                            return popup;
                        }

                        next.push_back(child);
                    }
                }

                level = std::move(next);
            }

            return nullptr;
        }

        // The element that draws a template pop-up's fill, found by its name in WinUI's templates:
        // PopupBorder (ComboBox), SuggestionsContainer (AutoSuggestBox) or PopupContentRoot
        // (NumberBox). It's usually the pop-up's child, but a ComboBox swaps its pop-up's child for a
        // Canvas of its own, holding the list's border beside a layer that closes the list when a
        // click lands outside it.
        inline winrt::Microsoft::UI::Xaml::FrameworkElement PopupFill(_In_ winrt::Microsoft::UI::Xaml::Controls::Primitives::Popup const& popup)
        {
            namespace xaml = winrt::Microsoft::UI::Xaml;

            std::vector<xaml::UIElement> level{ popup.Child() };

            for (int depth = 0; depth < 3 && !level.empty(); ++depth)
            {
                std::vector<xaml::UIElement> next{};

                for (auto const& element : level)
                {
                    if (element == nullptr)
                    {
                        continue;
                    }

                    if (auto const named = element.try_as<xaml::FrameworkElement>())
                    {
                        auto const name = named.Name();

                        if (name == L"PopupBorder" || name == L"SuggestionsContainer" || name == L"PopupContentRoot")
                        {
                            return named;
                        }
                    }

                    if (auto const panel = element.try_as<xaml::Controls::Panel>())
                    {
                        for (auto const& child : panel.Children())
                        {
                            next.push_back(child);
                        }
                    }
                    else if (auto const border = element.try_as<xaml::Controls::Border>())
                    {
                        next.push_back(border.Child());
                    }
                }

                level = std::move(next);
            }

            return nullptr;
        }

        // A template pop-up filled to match the menus, in whichever theme its control is shown.
        // In high contrast it goes back to its template's own fill.
        inline void FillTemplatePopup(_In_ winrt::Microsoft::UI::Xaml::Controls::Primitives::Popup const& popup, _In_ wchar_t const* key)
        {
            namespace xaml = winrt::Microsoft::UI::Xaml;

            if (popup == nullptr)
            {
                return;
            }

            auto const fill = PopupFill(popup);
            auto const border = fill.try_as<xaml::Controls::Border>();
            auto const panel = fill.try_as<xaml::Controls::Panel>();

            if (border == nullptr && panel == nullptr)
            {
                return;
            }

            auto const dark = ThemeBrush(L"Default", key);
            auto const light = ThemeBrush(L"Light", key);

            if (dark == nullptr || light == nullptr)
            {
                return;
            }

            auto const current = border != nullptr ? border.Background() : panel.Background();

            if (winrt::Windows::UI::ViewManagement::AccessibilitySettings{}.HighContrast())
            {
                if (current == dark || current == light)
                {
                    if (border != nullptr)
                    {
                        border.ClearValue(xaml::Controls::Border::BackgroundProperty());
                    }
                    else
                    {
                        panel.ClearValue(xaml::Controls::Panel::BackgroundProperty());
                    }
                }

                return;
            }

            // The pop-up itself is in the window's visual tree, so it knows the window's theme.
            auto const wanted = popup.ActualTheme() == xaml::ElementTheme::Light ? light : dark;

            if (current != wanted)
            {
                if (border != nullptr)
                {
                    border.Background(wanted);
                }
                else
                {
                    panel.Background(wanted);
                }
            }
        }

        // The pop-up of the control that's taking focus, if it's one of these.
        inline void FillForFocus(_In_ winrt::Microsoft::UI::Xaml::DependencyObject const& focused)
        {
            namespace xaml = winrt::Microsoft::UI::Xaml;

            if (focused == nullptr)
            {
                return;
            }

            // An item in a ComboBox's open list.
            if (auto const item = focused.try_as<xaml::Controls::ComboBoxItem>())
            {
                if (auto const combo = xaml::Controls::ItemsControl::ItemsControlFromItemContainer(item).try_as<xaml::Controls::ComboBox>())
                {
                    FillTemplatePopup(TemplatePopup(combo), ComboBoxFillKey);
                }

                return;
            }

            // A Hyperlink, say, isn't in the visual tree to look up from.
            if (focused.try_as<xaml::UIElement>() == nullptr)
            {
                return;
            }

            // The control, or the text box in its template, two levels below it.
            auto element = focused;

            for (int depth = 0; depth < 3 && element != nullptr; ++depth)
            {
                wchar_t const* key = nullptr;

                if (element.try_as<xaml::Controls::ComboBox>())
                {
                    key = ComboBoxFillKey;
                }
                else if (element.try_as<xaml::Controls::AutoSuggestBox>())
                {
                    key = AutoSuggestBoxFillKey;
                }
                else if (element.try_as<xaml::Controls::NumberBox>())
                {
                    key = NumberBoxFillKey;
                }

                if (key != nullptr)
                {
                    FillTemplatePopup(TemplatePopup(element), key);
                    return;
                }

                element = xaml::Media::VisualTreeHelper::GetParent(element);
            }
        }
    }

    // Whether this thread's windows give their pop-ups the menus' look.
    inline bool MenuBackdropsInUse() noexcept
    {
        return details::MenuBackdropsOn;
    }

    // The window color the pop-ups' fill is worked out from: the color picked in Appearance, if
    // one is. The window chrome calls this each time it applies the window's color, so a tool
    // doesn't need to.
    inline void SetPopupWindowColor(bool useCustom, uint32_t argb) noexcept
    {
        details::WindowColorIsCustom = useCustom;
        details::WindowColorArgb = argb;

        if (details::MenuBackdropsOn)
        {
            details::ApplyPopupFill();
        }
    }

    // Call for each Flyout before it opens. Calling it again does nothing, so a flyout on a button in
    // a list's rows can be given it whenever the button is clicked, which comes just before it opens.
    inline void UseMenuBackdrop(_In_ winrt::Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase const& flyout) noexcept
    {
        try
        {
            namespace xaml = winrt::Microsoft::UI::Xaml;

            if (flyout == nullptr || flyout.SystemBackdrop() != nullptr)
            {
                return;
            }

            auto const backdrop = details::MenuResource(L"MenuFlyoutSystemBackdrop").try_as<xaml::Media::SystemBackdrop>();
            auto const background = details::MenuResource(L"MenuFlyoutPresenterBackground");

            if (backdrop == nullptr || background == nullptr)
            {
                return;
            }

            flyout.ShouldConstrainToRootBounds(false);
            flyout.SystemBackdrop(backdrop);

            // The presenter's own fill would cover the backdrop. The new style is based on the one
            // the flyout already has, so its size limits stay, or on WinUI's, so its corners,
            // border and padding stay.
            if (auto const plain = flyout.try_as<xaml::Controls::Flyout>())
            {
                auto basis = plain.FlyoutPresenterStyle();

                if (basis == nullptr)
                {
                    basis = details::MenuResource(L"DefaultFlyoutPresenterStyle").try_as<xaml::Style>();
                }

                xaml::Style style{ winrt::Windows::UI::Xaml::Interop::TypeName{ L"Microsoft.UI.Xaml.Controls.FlyoutPresenter", winrt::Windows::UI::Xaml::Interop::TypeKind::Metadata } };

                if (basis != nullptr)
                {
                    style.BasedOn(basis);
                }

                style.Setters().Append(xaml::Setter{ xaml::Controls::Control::BackgroundProperty(), background });
                plain.FlyoutPresenterStyle(style);
            }
        }
        catch (...)
        {
        }
    }

    // A flyout that's a window of its own is kept on the screen, not inside the tool's window, so
    // one opened from near an edge of the window can open off it. This points it toward the middle
    // of the window: below an anchor in the window's top half and above one in the bottom half,
    // lined up with the anchor's edge nearer the middle. Call before it opens.
    inline void OpenTowardWindowMiddle(
        _In_ winrt::Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase const& flyout,
        _In_ winrt::Microsoft::UI::Xaml::FrameworkElement const& anchor) noexcept
    {
        try
        {
            auto const root = anchor.XamlRoot();

            if (root == nullptr)
            {
                return;
            }

            auto const center = anchor.TransformToVisual(nullptr).TransformPoint(winrt::Windows::Foundation::Point{
                static_cast<float>(anchor.ActualWidth() / 2.0), static_cast<float>(anchor.ActualHeight() / 2.0) });
            auto const size = root.Size();
            auto const right = center.X > size.Width / 2.0f;
            auto const below = center.Y < size.Height / 2.0f;

            using placement = winrt::Microsoft::UI::Xaml::Controls::Primitives::FlyoutPlacementMode;
            flyout.Placement(below ? (right ? placement::BottomEdgeAlignedRight : placement::BottomEdgeAlignedLeft)
                : (right ? placement::TopEdgeAlignedRight : placement::TopEdgeAlignedLeft));
        }
        catch (...)
        {
        }
    }

    // The same, worked out each time the flyout opens, from wherever its anchor is then: for a
    // flyout on a button, which opens it by itself, or one whose anchor can move. Call once.
    inline void AlwaysOpenTowardWindowMiddle(_In_ winrt::Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase const& flyout) noexcept
    {
        try
        {
            // A flyout reads its placement after raising Opening.
            flyout.Opening([](winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const&)
            {
                if (auto const opening = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase>())
                {
                    if (auto const target = opening.Target())
                    {
                        OpenTowardWindowMiddle(opening, target);
                    }
                }
            });
        }
        catch (...)
        {
        }
    }

    // Call once on the UI thread, as the first window starts.
    inline void UseMenuBackdropForLists()
    {
        if (details::MenuBackdropsOn)
        {
            return;
        }

        details::MenuBackdropsOn = true;
        details::ApplyPopupFill();

        namespace xaml = winrt::Microsoft::UI::Xaml;

        // Getting focus comes before a click opens a list. Got focus comes after, when a control
        // that took focus as its window or dialog opened has its template.
        xaml::Input::FocusManager::GettingFocus([](winrt::Windows::Foundation::IInspectable const&, xaml::Input::GettingFocusEventArgs const& args)
        {
            try
            {
                details::FillForFocus(args.NewFocusedElement());
            }
            catch (...)
            {
            }
        });

        xaml::Input::FocusManager::GotFocus([](winrt::Windows::Foundation::IInspectable const&, xaml::Input::FocusManagerGotFocusEventArgs const& args)
        {
            try
            {
                details::FillForFocus(args.NewFocusedElement());
            }
            catch (...)
            {
            }
        });
    }

    namespace details
    {
        inline winrt::Windows::UI::Color Mix(winrt::Windows::UI::Color from, winrt::Windows::UI::Color to, double amount) noexcept
        {
            auto const blend = [amount](uint8_t a, uint8_t b) { return static_cast<uint8_t>(std::lround(a + (static_cast<double>(b) - a) * amount)); };
            return winrt::Windows::UI::Color{ 255, blend(from.R, to.R), blend(from.G, to.G), blend(from.B, to.B) };
        }

        inline double Luminance(winrt::Windows::UI::Color color) noexcept
        {
            return (0.2126 * color.R + 0.7152 * color.G + 0.0722 * color.B) / 255.0;
        }
    }

    // Tooltips in the window's color. WinUI fills a tooltip with a neutral gray, ToolTipBackground,
    // and gives it no backdrop. With a background color picked in Appearance, a tooltip takes that
    // color lifted a little toward white, as the menus' acrylic lifts it, so it stands out from
    // the window as a menu does. Lifted more if the text on it would be hard to read. Without a
    // picked color it keeps WinUI's gray. The brushes go in the app's dark and light theme
    // dictionaries, so high contrast keeps its own colors, and they stay there, so a tooltip
    // already made changes color with them. Call as the window starts and whenever its color
    // changes.
    inline void TintToolTips(bool useCustom, uint32_t argb) noexcept
    {
        try
        {
            winrt::Windows::UI::Color const custom{ 255, static_cast<uint8_t>((argb >> 16) & 0xFF), static_cast<uint8_t>((argb >> 8) & 0xFF), static_cast<uint8_t>(argb & 0xFF) };
            winrt::Windows::UI::Color const white{ 255, 255, 255, 255 };

            // WinUI's own tooltip gray in each theme (SystemChromeMediumLowColor).
            winrt::Windows::UI::Color const darkGray{ 255, 0x2B, 0x2B, 0x2B };
            winrt::Windows::UI::Color const lightGray{ 255, 0xF2, 0xF2, 0xF2 };

            auto dark = useCustom ? details::Mix(custom, white, 0.10) : darkGray;
            auto light = useCustom ? details::Mix(custom, white, 0.35) : lightGray;

            // Light text needs a dark tooltip, and dark text a light one.
            if (details::Luminance(dark) > 0.35)
            {
                dark = details::Mix(dark, darkGray, 0.6);
            }

            if (details::Luminance(light) < 0.6)
            {
                light = details::Mix(light, lightGray, 0.7);
            }

            details::SetThemeBrush(L"Default", L"ToolTipBackground", dark);
            details::SetThemeBrush(L"Light", L"ToolTipBackground", light);
        }
        catch (...)
        {
        }
    }
}
