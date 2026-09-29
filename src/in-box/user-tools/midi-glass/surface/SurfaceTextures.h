// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pictures a theme lays over the page and its sections, and the noise a fine grain is made of.
// Needs composition and Win2D, so it is not part of the pure surface layer.

#include "ThemeModel.h"

namespace glass
{
    // A picture's pixels: premultiplied BGRA, top row first.
    struct TexturePixels
    {
        int32_t Width{ 0 };
        int32_t Height{ 0 };
        std::vector<uint8_t> Bgra{};
    };

    // Whether a picture of this name ships inside the app, for a theme that ships with it.
    bool IsBuiltInThemePicture(_In_ std::wstring const& fileName) noexcept;

    // A picture a theme names: a file in the themes folder, or one that ships inside the app.
    // Decoded once and kept. Null when there is no such picture or it cannot be read.
    std::shared_ptr<TexturePixels const> LoadThemePicture(_In_ std::wstring const& fileName) noexcept;

    // A fine noise in every pixel, lightening and darkening this theme's deck by about a tenth of
    // a level for every percent of its grain.
    std::shared_ptr<TexturePixels const> FineGrainPixels(_In_ Theme const& theme) noexcept;

    // The pixels, on this compositor.
    winrt::Microsoft::UI::Composition::CompositionSurfaceBrush MakeTextureBrush(
        _In_ winrt::Microsoft::UI::Composition::Compositor const& compositor,
        _In_ TexturePixels const& pixels);

    // A brush repeated at its own size from the top left over width by height, as a grid of
    // sprites sharing the one brush.
    winrt::Microsoft::UI::Composition::ContainerVisual BuildTiles(
        _In_ winrt::Microsoft::UI::Composition::Compositor const& compositor,
        _In_ winrt::Microsoft::UI::Composition::CompositionBrush const& brush,
        _In_ float tileWidth,
        _In_ float tileHeight,
        _In_ float width,
        _In_ float height);
}
