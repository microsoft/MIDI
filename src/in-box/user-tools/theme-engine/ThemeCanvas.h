// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A Win2D CanvasControl that layout sizes, at a display scale such as 175%, gets a size like
// 584.5714 DIPs: a whole number of pixels, but not of DIPs. At some of those sizes Win2D draws it a
// pixel off every second time it's drawn (microsoft/Win2D#989), so text on a canvas that's drawn
// again and again, as one is while a sequence plays, jumps up and down by a pixel.
//
// KeepWholePixels gives the element the largest size that fits where layout puts it and is a whole
// number of DIPs and of pixels (ThemePixels.h), at the top left of that space, and keeps it so as
// the layout and the scale change. Up to a few DIPs at its right and bottom are left to what's
// behind it.
//
// Like ThemePainter, this includes the app's pch.h, which has to bring in WinUI and Win2D.
//
// That isn't all of it. CanvasControl draws through a translation, to where XAML's surface keeps
// the picture, that's exact one time and a hair off the next. Shapes don't show it, but text does:
// Direct2D places glyphs on a different grid when the translation isn't whole, so every caption
// shakes by a quarter pixel, or by a whole one where it sits on a half. SteadyCanvas draws into a
// picture of its own, where the translation is exact, and copies that over pixel for pixel.

#include "ThemePixels.h"

#include <functional>

namespace themeengine
{
    void KeepWholePixels(_In_ winrt::Microsoft::UI::Xaml::FrameworkElement const& element);

    class SteadyCanvas
    {
    public:
        // In a CanvasControl's Draw handler: draw does the drawing, into the picture.
        void Draw(
            _In_ winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& sender,
            _In_ winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasDrawEventArgs const& args,
            _In_ std::function<void(winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const&)> const& draw);

    private:
        winrt::Microsoft::Graphics::Canvas::CanvasRenderTarget m_picture{ nullptr };
    };
}
