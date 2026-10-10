// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ThemePalette.h"

namespace midisequencer
{
    namespace
    {
        uint8_t Channel(double value) noexcept
        {
            return static_cast<uint8_t>(std::clamp(value, 0.0, 255.0) + 0.5);
        }
    }

    _Use_decl_annotations_
    Color Mix(Color a, double share, Color b) noexcept
    {
        auto const other = 1.0 - share;

        return Color{
            Channel(a.A * share + b.A * other),
            Channel(a.R * share + b.R * other),
            Channel(a.G * share + b.G * other),
            Channel(a.B * share + b.B * other) };
    }

    _Use_decl_annotations_
    Color WithAlpha(Color color, double alpha) noexcept
    {
        color.A = Channel(alpha * 255.0);
        return color;
    }

    _Use_decl_annotations_
    Color Desaturate(Color color, double saturation, double brightness) noexcept
    {
        // The luminance weights CSS's saturate() filter uses.
        auto const gray = 0.2126 * color.R + 0.7152 * color.G + 0.0722 * color.B;

        auto const blend = [&](double channel) { return (gray + (channel - gray) * saturation) * brightness; };

        return Color{ color.A, Channel(blend(color.R)), Channel(blend(color.G)), Channel(blend(color.B)) };
    }

    _Use_decl_annotations_
    Palette MakePalette(bool light) noexcept
    {
        Palette p{};
        p.Light = light;

        if (!light)
        {
            p.Text = Rgba(255, 255, 255);
            p.Text2 = Rgba(255, 255, 255, 0.786);
            p.Text3 = Rgba(255, 255, 255, 0.5442);
            p.Divider = Rgba(255, 255, 255, 0.0837);
            p.StrokeStrong = Rgba(255, 255, 255, 0.14);
            p.Accent = FromRgb(0x60CDFF);
            p.Play = FromRgb(0x6CCB5F);
            p.Record = FromRgb(0xFF5A5F);
            p.RecordText = FromRgb(0xFFB3B5);
            p.Mute = FromRgb(0xF7C948);
            p.Solo = FromRgb(0x60CDFF);
            p.Caution = FromRgb(0xFCE100);

            p.LaneBackground = Rgba(0, 0, 0, 0.18);
            p.FolderLaneBackground = Rgba(255, 255, 255, 0.025);
            p.LauncherBackground = Rgba(0, 0, 0, 0.10);
            p.LauncherEdge = Rgba(255, 255, 255, 0.14);
            p.PinnedTint = Rgba(96, 205, 255, 0.035);
            p.BarLine = Rgba(255, 255, 255, 0.075);
            p.BeatLine = Rgba(255, 255, 255, 0.028);

            p.RulerBackground = Rgba(255, 255, 255, 0.02);
            p.RulerBarLine = Rgba(255, 255, 255, 0.16);
            p.RulerBeatLine = Rgba(255, 255, 255, 0.06);
            p.LoopFill = Rgba(96, 205, 255, 0.38);
            p.LoopStroke = Rgba(96, 205, 255, 0.8);
            p.Playhead = Rgba(255, 255, 255);

            p.EmptySlotFill = Rgba(255, 255, 255, 0.018);
            p.EmptySlotStroke = Rgba(255, 255, 255, 0.05);
            p.EmptySlotGlyph = Rgba(255, 255, 255, 0.22);
            p.ArmedSlotGlyph = Rgba(255, 90, 95, 0.7);

            p.SelectionStroke = Rgba(255, 255, 255);
            p.CaptionText = Rgba(255, 255, 255);

            p.OverlayStripeA = Rgba(20, 20, 20, 0.78);
            p.OverlayStripeB = Rgba(20, 20, 20, 0.66);

            p.RollBackground = Rgba(0, 0, 0, 0.14);
            p.RollBlackKeyRow = Rgba(0, 0, 0, 0.20);
            p.RollWhiteKeyRow = Rgba(255, 255, 255, 0.025);
            p.RollOctaveLine = Rgba(255, 255, 255, 0.14);
            p.RollBarLine = Rgba(255, 255, 255, 0.22);
            p.RollBeatLine = Rgba(255, 255, 255, 0.10);
            p.RollStepLine = Rgba(255, 255, 255, 0.04);
            p.KeyWhite = FromRgb(0xD9D9D9);
            p.KeyBlack = FromRgb(0x1B1B1B);
            p.KeyEdge = FromRgb(0x8A8A8A);
            p.KeyLabel = FromRgb(0x333333);
            p.NoteEdge = Rgba(0, 0, 0, 0.55);
            p.LaneEdge = Rgba(255, 255, 255, 0.16);
            p.TempoLine = Rgba(255, 255, 255, 0.75);
            p.TempoLabelFill = FromRgb(0x262626);
            p.TempoLabelStroke = Rgba(255, 255, 255, 0.35);
            p.TempoLabelText = Rgba(255, 255, 255, 0.9);
            p.HandleFill = FromRgb(0x1E1E1E);
        }
        else
        {
            p.Text = Rgba(0, 0, 0, 0.894);
            p.Text2 = Rgba(0, 0, 0, 0.62);
            p.Text3 = Rgba(0, 0, 0, 0.47);
            p.Divider = Rgba(0, 0, 0, 0.0803);
            p.StrokeStrong = Rgba(0, 0, 0, 0.12);
            p.Accent = FromRgb(0x005FB8);
            p.Play = FromRgb(0x0F7B0F);
            p.Record = FromRgb(0xD13438);
            p.RecordText = FromRgb(0xA4262C);
            p.Mute = FromRgb(0xF7C948);
            p.Solo = FromRgb(0x60CDFF);
            p.Caution = FromRgb(0x9D5D00);

            p.LaneBackground = Rgba(0, 0, 0, 0.022);
            p.FolderLaneBackground = Rgba(0, 0, 0, 0.02);
            p.LauncherBackground = Rgba(0, 0, 0, 0.018);
            p.LauncherEdge = Rgba(0, 0, 0, 0.12);
            p.PinnedTint = Rgba(0, 95, 184, 0.035);
            p.BarLine = Rgba(0, 0, 0, 0.085);
            p.BeatLine = Rgba(0, 0, 0, 0.03);

            p.RulerBackground = Rgba(0, 0, 0, 0.015);
            p.RulerBarLine = Rgba(0, 0, 0, 0.16);
            p.RulerBeatLine = Rgba(0, 0, 0, 0.06);
            p.LoopFill = Rgba(0, 95, 184, 0.22);
            p.LoopStroke = Rgba(0, 95, 184, 0.7);
            p.Playhead = FromRgb(0x1A1A1A);

            p.EmptySlotFill = Rgba(0, 0, 0, 0.02);
            p.EmptySlotStroke = Rgba(0, 0, 0, 0.07);
            p.EmptySlotGlyph = Rgba(0, 0, 0, 0.25);
            p.ArmedSlotGlyph = Rgba(209, 52, 56, 0.75);

            p.SelectionStroke = FromRgb(0x1A1A1A);
            p.CaptionText = FromRgb(0x1A1A1A);

            p.OverlayStripeA = Rgba(243, 243, 243, 0.86);
            p.OverlayStripeB = Rgba(243, 243, 243, 0.72);

            p.RollBackground = Rgba(0, 0, 0, 0.02);
            p.RollBlackKeyRow = Rgba(0, 0, 0, 0.06);
            p.RollWhiteKeyRow = Rgba(255, 255, 255, 0.5);
            p.RollOctaveLine = Rgba(0, 0, 0, 0.14);
            p.RollBarLine = Rgba(0, 0, 0, 0.22);
            p.RollBeatLine = Rgba(0, 0, 0, 0.10);
            p.RollStepLine = Rgba(0, 0, 0, 0.04);
            p.KeyWhite = FromRgb(0xFCFCFC);
            p.KeyBlack = FromRgb(0x2A2A2A);
            p.KeyEdge = FromRgb(0xA0A0A0);
            p.KeyLabel = FromRgb(0x444444);
            p.NoteEdge = Rgba(0, 0, 0, 0.45);
            p.LaneEdge = Rgba(0, 0, 0, 0.14);
            p.TempoLine = Rgba(0, 0, 0, 0.7);
            p.TempoLabelFill = FromRgb(0xFFFFFF);
            p.TempoLabelStroke = Rgba(0, 0, 0, 0.3);
            p.TempoLabelText = Rgba(0, 0, 0, 0.85);
            p.HandleFill = FromRgb(0xFFFFFF);
        }

        return p;
    }

    _Use_decl_annotations_
    Color ClipFill(Palette const& palette, Color trackColor) noexcept
    {
        return palette.Light ? Mix(trackColor, 0.20, FromRgb(0xFFFFFF)) : Mix(trackColor, 0.22, FromRgb(0x1C1C1C));
    }

    _Use_decl_annotations_
    Color ClipStroke(Palette const& palette, Color trackColor) noexcept
    {
        // color-mix(tc 80%, #000 10%) leaves 10% unaccounted for, which CSS takes as transparency.
        return palette.Light ? WithAlpha(Mix(trackColor, 0.8 / 0.9, FromRgb(0x000000)), 0.9) : WithAlpha(trackColor, 0.62);
    }

    _Use_decl_annotations_
    Color ClipCaption(Palette const& palette, Color trackColor) noexcept
    {
        return palette.Light ? Mix(trackColor, 0.48, FromRgb(0xFFFFFF)) : Mix(trackColor, 0.55, FromRgb(0x202020));
    }

    _Use_decl_annotations_
    Color CellFill(Palette const& palette, Color trackColor, bool playing) noexcept
    {
        if (palette.Light)
        {
            return Mix(trackColor, playing ? 0.46 : 0.26, FromRgb(0xFFFFFF));
        }

        return Mix(trackColor, playing ? 0.42 : 0.22, FromRgb(0x1E1E1E));
    }

    _Use_decl_annotations_
    Color CellStroke(Palette const& palette, Color trackColor) noexcept
    {
        return palette.Light ? WithAlpha(Mix(trackColor, 0.75 / 0.85, FromRgb(0x000000)), 0.85) : WithAlpha(trackColor, 0.5);
    }

    _Use_decl_annotations_
    Color SummaryFill(Palette const&, Color trackColor) noexcept
    {
        return WithAlpha(trackColor, 0.14);
    }

    _Use_decl_annotations_
    Color SummaryStroke(Palette const&, Color trackColor) noexcept
    {
        return WithAlpha(trackColor, 0.35);
    }
}
