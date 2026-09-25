// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "DeckBrush.h"
#include "SurfaceColors.h"

namespace glass
{
    namespace
    {
        winrt::Windows::UI::Color ToColor(_In_ ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(color.A, color.R, color.G, color.B);
        }
    }

    _Use_decl_annotations_
    media::Brush MakeDeckBrush(ThemeDeck const& deck)
    {
        if (deck.Kind != DeckKind::Gradient)
        {
            // An image deck is drawn over its color by the surface, so the color is right here
            // for that case too.
            return media::SolidColorBrush(ToColor(deck.Color));
        }

        media::RadialGradientBrush brush{};

        // Lit from above and slightly off the top edge, so the fall-off reaches the bottom
        // corners of a wide page instead of stopping a third of the way down.
        brush.MappingMode(media::BrushMappingMode::RelativeToBoundingBox);
        brush.Center({ 0.5f, -0.10f });
        brush.GradientOrigin({ 0.5f, -0.10f });
        brush.RadiusX(1.30f);
        brush.RadiusY(1.25f);

        media::GradientStop top{};
        top.Color(ToColor(deck.Color));
        top.Offset(0.0);

        // The turn happens well before the edge rather than evenly across it, which is what
        // gives a deck a floor instead of a vignette.
        media::GradientStop knee{};
        knee.Color(ToColor(BlendOver(deck.Color, deck.GradientEndColor, 0.55)));
        knee.Offset(0.58);

        media::GradientStop floor{};
        floor.Color(ToColor(deck.GradientEndColor));
        floor.Offset(1.0);

        brush.GradientStops().Append(top);
        brush.GradientStops().Append(knee);
        brush.GradientStops().Append(floor);

        return brush;
    }
}
