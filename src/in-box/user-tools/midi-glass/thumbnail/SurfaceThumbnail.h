// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <string>

#include "LayoutModel.h"
#include "ThemeModel.h"

namespace glass
{
    // A card drawn by the real surface: the layout's first page built by the same renderer the
    // runtime uses, out of sight, captured and written as a PNG. A card then shows exactly what
    // the layout looks like when it opens - knob faces, lamps, wells and all - rather than a
    // second drawing of it that has to be kept in step with the first by hand.
    //
    // XAML only renders what is in a live window, so the host has to be a panel in one that is
    // on screen. The page is built inside it far off to one side, captured and taken down again.
    // Call it for one layout at a time, on the UI thread.
    //
    // False when the capture could not be made or came back empty. The caller then falls back
    // to the plain drawing in ThumbnailRenderer, which needs no window at all.
    winrt::Windows::Foundation::IAsyncOperation<bool> RenderSurfaceThumbnailAsync(
        _In_ winrt::Microsoft::UI::Xaml::Controls::Panel host,
        _In_ LayoutDocument document,
        _In_ Theme theme,
        _In_ int32_t imageWidth,
        _In_ int32_t imageHeight,
        _In_ std::wstring filePath);
}
