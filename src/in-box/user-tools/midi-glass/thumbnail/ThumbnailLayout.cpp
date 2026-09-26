// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and Win2D.

#include "ThumbnailLayout.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        // A control smaller than this after scaling still gets drawn at this size. A 1280 page
        // shrunk to a 240 wide card puts a fader at under 8 px; letting it round to nothing would
        // make a dense layout look empty, which is the opposite of what the card is for.
        constexpr double SmallestDrawnSize = 1.5;

        ThemeColor Darken(_In_ ThemeColor const& color, _In_ double amount) noexcept
        {
            auto const scale = std::clamp(1.0 - amount, 0.0, 1.0);

            return
            {
                static_cast<uint8_t>(std::lround(color.R * scale)),
                static_cast<uint8_t>(std::lround(color.G * scale)),
                static_cast<uint8_t>(std::lround(color.B * scale)),
                color.A
            };
        }

        ThemeColor Lighten(_In_ ThemeColor const& color, _In_ double amount) noexcept
        {
            auto const weight = std::clamp(amount, 0.0, 1.0);

            auto const mix = [weight](uint8_t channel)
                {
                    return static_cast<uint8_t>(std::clamp(
                        std::lround(channel + (255 - channel) * weight), 0L, 255L));
                };

            return { mix(color.R), mix(color.G), mix(color.B), color.A };
        }

        ThemeColor HueForControl(_In_ Control const& control, _In_ Theme const& theme) noexcept
        {
            if (control.HueSlot >= 0 && control.HueSlot < ThemeHueSlotCount)
            {
                return theme.HueSlots[static_cast<size_t>(control.HueSlot)];
            }

            // A literal color is stored as text on the control and is not parsed here: the
            // document layer keeps it as the customer typed it. A card falls back to the first
            // slot rather than guessing, which is wrong in the same direction every time instead
            // of unpredictably.
            return theme.HueSlots[0];
        }
    }

    _Use_decl_annotations_
    ThumbnailPlan PlanThumbnail(
        LayoutDocument const& document,
        Theme const& theme,
        int32_t imageWidth,
        int32_t imageHeight,
        size_t pageIndex) noexcept
    {
        ThumbnailPlan plan{};

        plan.Width = (std::max)(imageWidth, 1);
        plan.Height = (std::max)(imageHeight, 1);
        plan.DeckColor = theme.Deck.Color;
        plan.DeckTopColor = Lighten(theme.Deck.Color, 0.08);
        plan.DeckBottomColor = Darken(theme.Deck.Color, 0.22);
        plan.SurroundColor = Darken(theme.Deck.Color, 0.45);
        plan.PlateOpacity = std::clamp(theme.GlassTintPercent / 100.0, 0.0, 1.0);

        if (document.PageWidth <= 0 || document.PageHeight <= 0)
        {
            return plan;
        }

        // Letterboxed, never stretched, for the same reason the runtime does it: a control that
        // changes shape between one view and another stops being recognizable.
        auto const scale = (std::min)(
            static_cast<double>(plan.Width) / document.PageWidth,
            static_cast<double>(plan.Height) / document.PageHeight);

        auto const pageWidth = document.PageWidth * scale;
        auto const pageHeight = document.PageHeight * scale;

        plan.PageBounds =
        {
            (plan.Width - pageWidth) / 2.0,
            (plan.Height - pageHeight) / 2.0,
            pageWidth,
            pageHeight
        };

        plan.CornerRadius = (std::max)(theme.CornerRadius * scale, 0.5);

        if (pageIndex >= document.Pages.size())
        {
            return plan;
        }

        auto const& page = document.Pages[pageIndex];

        plan.Items.reserve(page.Controls.size());

        for (auto const& control : page.Controls)
        {
            if (control.Width <= 0 || control.Height <= 0)
            {
                continue;
            }

            // Off-page controls are not part of what ships, so a card does not show them.
            if (control.X < 0 ||
                control.Y < 0 ||
                control.X + control.Width > document.PageWidth ||
                control.Y + control.Height > document.PageHeight)
            {
                continue;
            }

            ThumbnailItem item{};

            item.Kind = control.Kind;
            item.Hue = HueForControl(control, theme);

            item.Bounds =
            {
                plan.PageBounds.X + control.X * scale,
                plan.PageBounds.Y + control.Y * scale,
                (std::max)(control.Width * scale, SmallestDrawnSize),
                (std::max)(control.Height * scale, SmallestDrawnSize)
            };

            plan.Items.push_back(item);
        }

        return plan;
    }
}
