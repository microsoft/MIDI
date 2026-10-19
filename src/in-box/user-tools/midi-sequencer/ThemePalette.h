// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The colors the drawn parts of the window use: the timeline, the launcher, the ruler and the
// clip editor. They come from the theme in use (Themes\*.json, through the theme engine), each
// field a color of the theme's own by the field's name ("text2", "laneBackground"). The values
// MakePalette(light) builds are Night drive's, the comps' tokens (design/seq.css): the colors a
// theme leaves out, and the whole palette if a theme can't be read.
//
// Plain C++ and WinRT, without the app's pch, so the unit tests can check Night drive against it.

#include <sal.h>

#include <cstdint>
#include <memory>

#include <winrt/Windows.UI.h>

#include "ThemeResolver.h"

namespace midisequencer
{
    using Color = winrt::Windows::UI::Color;

    constexpr Color Rgba(uint8_t r, uint8_t g, uint8_t b, double alpha = 1.0) noexcept
    {
        return Color{ static_cast<uint8_t>(alpha * 255.0 + 0.5), r, g, b };
    }

    constexpr Color FromRgb(uint32_t rgb, double alpha = 1.0) noexcept
    {
        return Rgba(static_cast<uint8_t>((rgb >> 16) & 0xFF), static_cast<uint8_t>((rgb >> 8) & 0xFF), static_cast<uint8_t>(rgb & 0xFF), alpha);
    }

    // CSS color-mix(in srgb, a share%, b): a straight blend of two opaque colors.
    Color Mix(_In_ Color a, _In_ double share, _In_ Color b) noexcept;

    Color WithAlpha(_In_ Color color, _In_ double alpha) noexcept;

    // What CSS saturate() and brightness() do, for muted clips.
    Color Desaturate(_In_ Color color, _In_ double saturation, _In_ double brightness) noexcept;

    struct Palette
    {
        bool Light{ false };

        // The theme the colors came from, for the colors that depend on a track and for the
        // parts a theme draws as more than a color. Null for the built-in palette.
        std::shared_ptr<themeengine::ResolvedTheme const> Theme{};

        Color Text{};
        Color Text2{};
        Color Text3{};
        Color Divider{};
        Color StrokeStrong{};
        Color Accent{};
        Color Play{};
        Color Record{};
        Color RecordText{};
        Color Mute{};
        Color Solo{};
        Color Caution{};

        Color LaneBackground{};
        Color FolderLaneBackground{};
        Color LauncherBackground{};
        Color LauncherEdge{};
        Color BarLine{};
        Color BeatLine{};

        // A grid engraved in the timeline: a light line beside each bar line, and a seam under
        // each lane. Transparent in the comps' own look, which has neither.
        Color BarLineLight{};
        Color LaneSeam{};
        Color LaneSeamLight{};

        Color RulerBackground{};
        Color RulerBarLine{};
        Color RulerBeatLine{};
        Color LoopFill{};
        Color LoopStroke{};
        Color Playhead{};

        Color EmptySlotFill{};
        Color EmptySlotStroke{};
        Color EmptySlotGlyph{};
        Color ArmedSlotGlyph{};

        Color SelectionStroke{};

        Color OverlayStripeA{};
        Color OverlayStripeB{};

        // the clip editor
        Color RollBackground{};
        Color RollBlackKeyRow{};
        Color RollWhiteKeyRow{};
        Color RollOctaveLine{};
        Color RollBarLine{};
        Color RollBeatLine{};
        Color RollStepLine{};
        Color KeyWhite{};
        Color KeyBlack{};
        Color KeyEdge{};
        Color KeyLabel{};
        Color NoteEdge{};
        Color LaneEdge{};
        Color TempoLine{};
        Color TempoLabelFill{};
        Color TempoLabelStroke{};
        Color TempoLabelText{};
        Color HandleFill{};
    };

    Palette MakePalette(_In_ bool light) noexcept;

    // The palette of a theme worked out for light or dark. Never throws; a color the theme doesn't
    // have keeps Night drive's.
    Palette MakePalette(_In_ std::shared_ptr<themeengine::ResolvedTheme const> const& theme) noexcept;

    Color ToUiColor(_In_ themeengine::Color const& color) noexcept;
    themeengine::Color ToThemeColor(_In_ Color const& color) noexcept;

    // A track's color (its own, as the sequence keeps it) as the theme shows it: a theme made of a
    // material can have caps of its own for the app's colors.
    Color ThemedTrackColor(_In_ Palette const& palette, _In_ Color trackColor) noexcept;

    // A track color as it appears on each kind of drawn surface, worked out the way the comps'
    // CSS mixes it.
    Color ClipFill(_In_ Palette const& palette, _In_ Color trackColor) noexcept;
    Color ClipStroke(_In_ Palette const& palette, _In_ Color trackColor) noexcept;
    Color ClipCaption(_In_ Palette const& palette, _In_ Color trackColor) noexcept;
    Color CellFill(_In_ Palette const& palette, _In_ Color trackColor, _In_ bool playing) noexcept;
    Color CellStroke(_In_ Palette const& palette, _In_ Color trackColor) noexcept;
    Color SummaryFill(_In_ Palette const& palette, _In_ Color trackColor) noexcept;
    Color SummaryStroke(_In_ Palette const& palette, _In_ Color trackColor) noexcept;
}
