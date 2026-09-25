// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "PageTemplates.h"

#include <cmath>

namespace glass
{
    namespace
    {
        // Sizes are quoted at the reference page and scaled from there. Taken from the design
        // document for pad, knob, fader and XY pad; the rest follow the same proportions.
        ControlSize ReferenceSize(_In_ ControlKind kind) noexcept
        {
            switch (kind)
            {
            case ControlKind::Knob:     return { 56, 56 };
            case ControlKind::Encoder:  return { 56, 56 };
            case ControlKind::Pad:      return { 56, 56 };
            case ControlKind::Fader:    return { 40, 180 };
            case ControlKind::XYPad:    return { 240, 240 };
            case ControlKind::Button:   return { 96, 40 };
            case ControlKind::Toggle:   return { 96, 40 };
            case ControlKind::Meter:    return { 24, 180 };
            case ControlKind::Lamp:     return { 32, 32 };
            case ControlKind::Readout:  return { 120, 40 };
            case ControlKind::Label:    return { 120, 24 };
            case ControlKind::Image:    return { 120, 120 };
            case ControlKind::PageTab:  return { 120, 36 };

            // Big enough to hold a row of four faders and their labels, which is the smallest
            // thing anybody draws a box around.
            case ControlKind::Panel:    return { 280, 220 };
            }

            return { 56, 56 };
        }

        double Diagonal(_In_ double width, _In_ double height) noexcept
        {
            return std::sqrt(width * width + height * height);
        }
    }

    std::vector<PageTemplate> const& PageTemplates() noexcept
    {
        static std::vector<PageTemplate> const templates
        {
            { L"PageTemplateTablet", 1280, 800 },
            { L"PageTemplateFullHd", 1920, 1080 },
            { L"PageTemplateQuadHd", 2560, 1440 },
            { L"PageTemplateSurface", 2736, 1824 },
            { L"PageTemplateClassic", 1024, 768 },
            { L"PageTemplatePortrait", 1080, 1920 },
        };

        return templates;
    }

    _Use_decl_annotations_
    int32_t QuantizePixels(double value) noexcept
    {
        if (!std::isfinite(value))
        {
            return PixelQuantum;
        }

        auto const steps = static_cast<int32_t>(std::lround(value / PixelQuantum));

        return steps < 1 ? PixelQuantum : steps * PixelQuantum;
    }

    _Use_decl_annotations_
    ControlSize DefaultControlSize(ControlKind kind, int32_t pageWidth, int32_t pageHeight) noexcept
    {
        auto const reference = ReferenceSize(kind);

        if (pageWidth <= 0 || pageHeight <= 0)
        {
            return reference;
        }

        auto const referenceDiagonal = Diagonal(ReferencePageWidth, ReferencePageHeight);
        auto const pageDiagonal = Diagonal(pageWidth, pageHeight);

        auto const scale = std::sqrt(pageDiagonal / referenceDiagonal);

        return
        {
            QuantizePixels(reference.Width * scale),
            QuantizePixels(reference.Height * scale)
        };
    }
}
