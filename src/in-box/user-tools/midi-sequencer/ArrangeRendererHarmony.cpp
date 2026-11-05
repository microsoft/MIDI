// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The Key and chords row (Pete's proposal of November 1 2026): a band along the top with each
// key, and the chords under it as blocks, with their names and, when there's room, their Roman
// numerals. A chord from outside the key has a star and a warm outline.

#include "pch.h"
#include "ArrangeRenderer.h"
#include "Harmony.h"

namespace midisequencer
{
    namespace
    {
        constexpr float BlockGap = 2.0f;
        constexpr float BlockInset = 6.0f;
        constexpr float BlockRadius = 4.0f;

        // Narrower than these, a block shows less: no numeral, then no name.
        constexpr float NumeralWidth = 30.0f;
        constexpr float NameWidth = 14.0f;
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawHarmonyRow(canvas::CanvasDrawingSession const& ds, float top, float height, float width, ArrangeDrawContext const& context)
    {
        auto const& doc = *context.Doc;
        auto const& colors = *context.Colors;

        if (!doc.Harmony)
        {
            return;
        }

        auto const& harmony = *doc.Harmony;
        auto const bandHeight = static_cast<float>(HarmonyKeyBandHeight);
        auto const blockTop = top + bandHeight + 2.0f;
        auto const blockHeight = std::max(1.0f, height - bandHeight - 5.0f);

        // The keys, each until the next.
        if (harmony.Keys.empty())
        {
            ds.DrawText(context.NoKeyHint, 6.0f, top, std::max(1.0f, width - 12.0f), bandHeight, colors.Text3, m_badge);
        }

        for (size_t i = 0; i < harmony.Keys.size(); ++i)
        {
            auto const& key = harmony.Keys[i];
            auto const x0 = std::max(-1.0f, static_cast<float>(context.View.XAtTick(key.Tick)));
            auto const x1 = i + 1 < harmony.Keys.size() ? static_cast<float>(context.View.XAtTick(harmony.Keys[i + 1].Tick)) : width + 1.0f;

            if (x1 < 0 || x0 > width)
            {
                continue;
            }

            auto const fill = WithAlpha(i % 2 == 0 ? colors.Accent : colors.Play, colors.Light ? 0.16 : 0.22);
            ds.FillRectangle(x0, top + 1.0f, std::max(1.0f, x1 - x0), bandHeight - 1.0f, fill);
            ds.FillRectangle(x0, top + 1.0f, 1.5f, bandHeight - 1.0f, i % 2 == 0 ? colors.Accent : colors.Play);

            if (i == context.SelectedKey)
            {
                ds.DrawRectangle(x0 + 0.5f, top + 1.5f, std::max(1.0f, x1 - x0 - 1.0f), bandHeight - 2.0f, colors.SelectionStroke, 1.5f);
            }

            std::wstring label = NoteName(key.Root, KeyUsesFlats(key.Root, key.Scale));
            auto const scaleIndex = static_cast<size_t>(key.Scale);

            if (context.ScaleNames != nullptr && scaleIndex < context.ScaleNames->size())
            {
                label += L" ";
                label += (*context.ScaleNames)[scaleIndex];
            }

            ds.DrawText(winrt::hstring{ label }, x0 + 6.0f, top + 1.0f, std::max(1.0f, x1 - x0 - 8.0f), bandHeight - 1.0f, colors.Text, m_tagText);
        }

        if (harmony.Chords.empty())
        {
            ds.DrawText(context.EmptyHarmonyHint, 12.0f, blockTop, std::max(1.0f, width - 24.0f), blockHeight, colors.Text3, m_laneHint);
            return;
        }

        for (size_t i = 0; i < harmony.Chords.size(); ++i)
        {
            auto const& chord = harmony.Chords[i];
            auto const x0 = static_cast<float>(context.View.XAtTick(chord.Tick)) + BlockGap / 2.0f;
            auto const x1 = static_cast<float>(context.View.XAtTick(chord.Tick + chord.Length)) - BlockGap / 2.0f;

            if (x1 < 0)
            {
                continue;
            }

            if (x0 > width)
            {
                break;
            }

            auto const blockWidth = std::max(2.0f, x1 - x0);
            auto const key = KeyAt(harmony, chord.Tick);
            auto const outside = key != nullptr && !ChordIsInKey(chord, *key);
            auto const selected = i == context.SelectedChord;

            auto const fill = WithAlpha(outside ? colors.Caution : colors.Accent, colors.Light ? 0.12 : 0.18);
            auto const stroke = selected ? colors.SelectionStroke : outside ? colors.Caution : WithAlpha(colors.Accent, 0.6);

            ds.FillRoundedRectangle(x0, blockTop, blockWidth, blockHeight, BlockRadius, BlockRadius, fill);
            ds.DrawRoundedRectangle(x0 + 0.5f, blockTop + 0.5f, blockWidth - 1.0f, blockHeight - 1.0f, BlockRadius, BlockRadius, stroke, selected ? 2.0f : 1.0f);

            if (blockWidth < NameWidth)
            {
                continue;
            }

            auto const textWidth = std::max(1.0f, blockWidth - BlockInset - 2.0f);
            ds.DrawText(winrt::hstring{ ChordName(chord, key) }, x0 + BlockInset, blockTop + 3.0f, textWidth, 18.0f, colors.Text, m_chordName);

            if (key != nullptr && context.ShowNumerals && blockWidth >= NumeralWidth && blockHeight >= 32.0f)
            {
                auto numeral = RomanNumeral(chord, *key);

                if (outside)
                {
                    numeral += L" \u2605";
                }

                ds.DrawText(winrt::hstring{ numeral }, x0 + BlockInset, blockTop + 20.0f, textWidth, 14.0f, outside ? colors.Caution : colors.Text2, m_chordNumeral);
            }
        }
    }
}
