// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "SurfaceModel.h"

namespace gspike
{
    namespace
    {
        // Fixed sequence, so every mode and every run gets an identical page.
        struct Lcg
        {
            uint32_t State{ 0x5EED1234u };

            uint32_t Next() noexcept
            {
                State = State * 1664525u + 1013904223u;
                return State;
            }

            uint32_t Below(uint32_t limit) noexcept
            {
                return limit == 0 ? 0 : Next() % limit;
            }

            float Unit() noexcept
            {
                return static_cast<float>(Next() >> 8) / static_cast<float>(1u << 24);
            }
        };

        winrt::Windows::UI::Color Rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept
        {
            return winrt::Windows::UI::Color{ a, r, g, b };
        }
    }

    winrt::Windows::UI::Color HueColor(uint8_t slot) noexcept
    {
        // Studio Dark, the default theme in the design document.
        switch (slot % HueSlotCount)
        {
        case 0: return Rgb(0x4F, 0xC3, 0xF7);   // cyan
        case 1: return Rgb(0xFF, 0xC2, 0x47);   // amber
        case 2: return Rgb(0x9C, 0xD8, 0x4E);   // lime
        case 3: return Rgb(0xFF, 0x76, 0x8A);   // rose
        case 4: return Rgb(0xB3, 0x8C, 0xFF);   // violet
        default: return Rgb(0x6A, 0xE8, 0xC8);  // mint
        }
    }

    winrt::Windows::UI::Color DeckColor() noexcept { return Rgb(0x10, 0x11, 0x14); }
    winrt::Windows::UI::Color PlateColor() noexcept { return Rgb(0x1B, 0x1D, 0x22, 0xDB); }
    winrt::Windows::UI::Color TrackColor() noexcept { return Rgb(0x00, 0x00, 0x00, 0x9A); }

    PageModel BuildPage(uint32_t controlCount, uint32_t animatedCount)
    {
        PageModel page{};
        page.Controls.reserve(controlCount);

        if (controlCount == 0)
        {
            return page;
        }

        // Pack into a grid that roughly fills a 1280 x 800 page, then let the kind decide how much
        // of its cell a control uses. Everything lands on a 4 px multiple, as the design requires.
        const uint32_t columns = std::max(1u, static_cast<uint32_t>(std::lround(
            std::sqrt(static_cast<double>(controlCount) * static_cast<double>(page.Width) / static_cast<double>(page.Height)))));
        const uint32_t rows = (controlCount + columns - 1) / columns;

        const float cellWidth = std::floor((page.Width - 16.0f) / static_cast<float>(columns) / 4.0f) * 4.0f;
        const float cellHeight = std::floor((page.Height - 16.0f) / static_cast<float>(rows) / 4.0f) * 4.0f;

        Lcg rng{};

        for (uint32_t i = 0; i < controlCount; i++)
        {
            const uint32_t column = i % columns;
            const uint32_t row = i / columns;

            ControlDescriptor d{};
            d.Kind = static_cast<ControlKind>(i % 4 == 1 ? 1 : (i % 4 == 2 ? 2 : 0));
            d.HueSlot = static_cast<uint8_t>(rng.Below(HueSlotCount));
            d.Value = rng.Unit();
            d.Group = static_cast<uint8_t>(column % 4);
            d.Channel = static_cast<uint8_t>(row % 16);
            d.ControllerNumber = static_cast<uint8_t>(16 + (i % 96));

            const float cellX = 8.0f + static_cast<float>(column) * cellWidth;
            const float cellY = 8.0f + static_cast<float>(row) * cellHeight;

            if (d.Kind == ControlKind::Fader)
            {
                d.Width = cellWidth - PlateInset * 2.0f;
                d.Height = cellHeight - PlateInset * 2.0f;
            }
            else
            {
                // knobs and pads are square
                const float side = std::floor((std::min(cellWidth, cellHeight) - PlateInset * 2.0f) / 4.0f) * 4.0f;
                d.Width = side;
                d.Height = side;
            }

            d.X = std::floor(cellX + (cellWidth - d.Width) * 0.5f);
            d.Y = std::floor(cellY + (cellHeight - d.Height) * 0.5f);

            page.Controls.push_back(d);
        }

        // Spread the animating controls across the page rather than clustering them, so the cost
        // is not concentrated in one corner of the visual tree.
        const uint32_t animating = std::min(animatedCount, controlCount);
        page.AnimatedIndices.reserve(animating);

        for (uint32_t i = 0; i < animating; i++)
        {
            const uint32_t index = static_cast<uint32_t>(
                (static_cast<uint64_t>(i) * controlCount) / std::max(1u, animating));

            page.AnimatedIndices.push_back(index);
            page.Controls[index].Animated = true;
        }

        return page;
    }
}
