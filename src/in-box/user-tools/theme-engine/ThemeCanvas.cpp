// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ThemeCanvas.h"

#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>

namespace themeengine
{
    namespace
    {
        namespace mux = ::winrt::Microsoft::UI::Xaml;

        bool Same(double a, double b) noexcept
        {
            // An unset size is NaN, which is never the same.
            return std::abs(a - b) < 0.001;
        }

        void Fit(mux::FrameworkElement const& element)
        {
            auto const root = element.XamlRoot();

            if (root == nullptr)
            {
                return;
            }

            // Where the parent put it, margins and all.
            auto const scale = root.RasterizationScale();
            auto const slot = mux::Controls::Primitives::LayoutInformation::GetLayoutSlot(element);
            auto const margin = element.Margin();

            auto const width = WholePixelLength(static_cast<double>(slot.Width) - margin.Left - margin.Right, scale);
            auto const height = WholePixelLength(static_cast<double>(slot.Height) - margin.Top - margin.Bottom, scale);

            if (!Same(element.Width(), width))
            {
                element.Width(width);
            }

            if (!Same(element.Height(), height))
            {
                element.Height(height);
            }
        }
    }

    _Use_decl_annotations_
    void KeepWholePixels(winrt::Microsoft::UI::Xaml::FrameworkElement const& element)
    {
        if (element == nullptr)
        {
            return;
        }

        element.HorizontalAlignment(mux::HorizontalAlignment::Left);
        element.VerticalAlignment(mux::VerticalAlignment::Top);

        // After every layout: its space may have changed, or the scale with the window's display.
        // Setting the size lays it out once more, and then there's nothing to change.
        element.LayoutUpdated([weak = winrt::make_weak(element)](winrt::Windows::Foundation::IInspectable const&, winrt::Windows::Foundation::IInspectable const&)
        {
            try
            {
                if (auto const strong = weak.get())
                {
                    Fit(strong);
                }
            }
            catch (...)
            {
            }
        });
    }

    _Use_decl_annotations_
    void SteadyCanvas::Draw(
        winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& sender,
        winrt::Microsoft::Graphics::Canvas::UI::Xaml::CanvasDrawEventArgs const& args,
        std::function<void(winrt::Microsoft::Graphics::Canvas::CanvasDrawingSession const&)> const& draw)
    {
        namespace wgc = ::winrt::Microsoft::Graphics::Canvas;

        auto const size = sender.Size();
        auto const dpi = sender.Dpi();
        auto const target = args.DrawingSession();

        if (size.Width <= 0.0f || size.Height <= 0.0f)
        {
            return;
        }

        // A new picture for a new size, display, or device.
        if (m_picture == nullptr || m_picture.Size() != size || m_picture.Dpi() != dpi || m_picture.Device() != sender.Device())
        {
            m_picture = wgc::CanvasRenderTarget{ sender, size.Width, size.Height, dpi };
        }

        {
            auto const ds = m_picture.CreateDrawingSession();
            ds.Clear(winrt::Microsoft::UI::Colors::Transparent());
            draw(ds);
        }

        // Pixel for pixel: a hair off still lands each on the same pixel.
        target.DrawImage(m_picture, 0.0f, 0.0f, winrt::Windows::Foundation::Rect{ 0.0f, 0.0f, size.Width, size.Height }, 1.0f,
            wgc::CanvasImageInterpolation::NearestNeighbor);
    }
}
