// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <string>

#include "ThumbnailLayout.h"

namespace glass
{
    // Draws a thumbnail plan to a PNG with no window and no compositor.
    //
    // This is the one place in the app that draws without XAML. It has to be: a card is needed
    // for a layout that has never been opened on this PC, and neither XAML nor composition will
    // render a frame without a window to render it into.
    //
    // The file goes under %LOCALAPPDATA% rather than next to the layout. Documents is perfectly
    // writable; the reasons are that the customer looks at that folder and derived files clutter
    // it, that a cache should not be synced or backed up, and that zipping the layouts folder to
    // send to a friend should contain layouts and nothing else. Deleting the whole cache must
    // never lose anything.
    struct ThumbnailResult
    {
        bool Succeeded{ false };
        std::wstring Detail{};
    };

    ThumbnailResult RenderThumbnailToFile(
        _In_ ThumbnailPlan const& plan,
        _In_ std::wstring const& filePath) noexcept;

    // The cache folder, created if it is not there. Empty when it cannot be created.
    std::wstring ThumbnailCacheFolder() noexcept;

    // Where this layout's card lives. The name is derived from the layout path, so two layouts
    // with the same file name in different folders do not fight over one entry.
    std::wstring ThumbnailPathForLayout(
        _In_ std::wstring const& layoutFilePath,
        _In_ int32_t imageWidth) noexcept;
}
