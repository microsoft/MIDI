// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    // Artwork the tools carry inside the executable, the same way they carry their icon. These
    // tools ship unpackaged, so an asset kept as a loose file would be one more thing that has
    // to arrive next to the exe; an RCDATA resource simply cannot go missing.
    class EmbeddedAssets
    {
    public:
        // Fills an Image from an SVG stored as an RCDATA resource.
        //
        // The Image must already have Width and Height, because the drawing art these tools use
        // is authored at 100% x 100% and so carries no size of its own for Stretch to scale
        // from. Those two values, scaled for the display, are what the SVG is rasterized at,
        // which keeps the size in the markup and keeps the result crisp on a high DPI screen.
        //
        // The decode finishes after this returns, so the picture appears a frame or two later.
        static void SetSvgFromResource(
            _In_ winrt::Microsoft::UI::Xaml::Controls::Image const& target,
            _In_ uint16_t const resourceId) noexcept;
    };
}
