// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <cstdint>
#include <string>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    // Everything on a surface is a multiple of this, so snapping always lands cleanly and a
    // control never ends up on a half pixel.
    constexpr int32_t PixelQuantum = 4;
    constexpr int32_t DefaultGridSize = 8;

    // A new layout starts from one of these rather than from an empty number box. A page has a
    // fixed pixel size and scales letterboxed, so the template is a real decision.
    struct PageTemplate
    {
        // Looked up in the resources for display. Not shown to a customer as it stands here.
        std::wstring ResourceKey{};
        int32_t Width{ 0 };
        int32_t Height{ 0 };
    };

    std::vector<PageTemplate> const& PageTemplates() noexcept;

    // Starting sizes, derived from the page rather than fixed, so a 1024 x 768 page gets chunkier
    // controls than a 2560 x 1440 one. See DefaultControlSize for how, and why it is a formula
    // rather than a table.
    struct ControlSize
    {
        int32_t Width{ 0 };
        int32_t Height{ 0 };
    };

    // The reference page every default size below is quoted against.
    constexpr int32_t ReferencePageWidth = 1280;
    constexpr int32_t ReferencePageHeight = 800;

    // A default size for this kind of control on a page of this size.
    //
    // The scale is the square root of the page's diagonal against the reference diagonal, which
    // is deliberately damped rather than linear. A bigger page gets somewhat bigger controls, but
    // they take up a smaller fraction of it — which is the behavior wanted, because fingers do
    // not get bigger with the monitor. A 1024 x 768 page ends up chunkier relative to itself than
    // a 2560 x 1440 page, without either one being absurd in absolute pixels.
    ControlSize DefaultControlSize(
        _In_ ControlKind kind,
        _In_ int32_t pageWidth,
        _In_ int32_t pageHeight) noexcept;

    // Rounds to the 4 px quantum, never below it.
    int32_t QuantizePixels(_In_ double value) noexcept;
}
