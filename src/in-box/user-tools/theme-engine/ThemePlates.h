// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Plates: a part's look drawn behind a XAML element, so the element looks the way the theme's CSS
// says. One Win2D canvas at the back of a panel draws, for each element it's given, the element's
// part in the states the element is in (hover, checked, pressed, disabled, then any the app adds),
// with shadows that reach past the element as a browser's do, and as see-through as the element.
//
// An element whose part the theme doesn't have is left as it is. One with a plate gives up its own
// background: a button's template becomes just its content, and a combo box's own fills go clear.
// It takes the part's text color (unless the app set one), font, size, weight and letter spacing,
// and the part's width and height when asked. All of that goes back when the theme changes to one
// without the part, or the element is taken off.
//
// Like ThemePainter, this includes the app's pch.h, which has to bring in Win2D.

#include "ThemePainter.h"

#include <functional>

namespace themeengine
{
    struct PlateOptions
    {
        // States the app adds after the element's own, such as "on" or "record"; later states win.
        // Asked whenever the plate is drawn or its text is styled.
        std::function<void(
            winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
            std::vector<std::wstring>& states)> States{};

        // The track color, for a part that uses one.
        std::optional<Color> Track{};

        // An element inside something that scrolls has its plate cut to that.
        winrt::Microsoft::UI::Xaml::FrameworkElement ClipTo{ nullptr };

        // The part's width and height size the element too, as a transport key's do.
        bool Sized{ false };

        // Draws what the plate has besides its part, such as a key's light, over it.
        std::function<void(
            winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            winrt::Microsoft::Graphics::Canvas::ICanvasResourceCreatorWithDpi const& creator,
            ThemePainter& painter,
            ResolvedTheme const& theme,
            ResolvedStyle const& style,
            PaintRect const& rect)> Decorate{};

        // Plates added together, such as rows that are built again, go together (Remove).
        std::wstring Group{};
    };

    class PlateLayer
    {
    public:
        PlateLayer();
        ~PlateLayer();

        PlateLayer(PlateLayer const&) = delete;
        PlateLayer& operator=(PlateLayer const&) = delete;

        // A canvas in the panel at the index, so it's drawn behind what comes after it. The caller
        // places it (a grid's row and column, say) through Surface.
        void Attach(
            _In_ winrt::Microsoft::UI::Xaml::Controls::Panel const& host,
            _In_ uint32_t index);

        winrt::Microsoft::UI::Xaml::FrameworkElement Surface() const noexcept;

        // The look to draw in, or null for none. True when an element's own resources changed,
        // which XAML only picks up when the theme changes (see XamlThemeResources::Apply).
        bool Theme(_In_ std::shared_ptr<ResolvedTheme const> theme);

        std::shared_ptr<ResolvedTheme const> Theme() const noexcept;

        // True when the element's own resources changed (as above).
        bool Add(
            _In_ winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
            _In_ std::wstring part,
            _In_ PlateOptions options = {});

        // Every element goes, with its own look back.
        void Clear() noexcept;

        // The elements added in the group go, with their own looks back.
        void Remove(_In_ std::wstring_view group) noexcept;

        // Each element's text styled again for its states, and the plates drawn again: for a state
        // the app keeps itself, such as a saved document.
        void Refresh() noexcept;

        // Drawn again, for a state only the drawing reads, or an element moved or faded without
        // a layout (a render transform, an opacity).
        void Invalidate() noexcept;

    private:
        struct State;
        std::shared_ptr<State> m_state{};
    };
}
