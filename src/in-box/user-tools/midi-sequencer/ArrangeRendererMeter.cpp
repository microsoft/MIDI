// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The Meter row (design section 37; Pete, November 7 2026, after Cubase's Signature track): a
// flag at the start of each bar where the meter changes, the first one too, each with its
// meter, such as 7/8.

#include "pch.h"
#include "ArrangeRenderer.h"

namespace midisequencer
{
    namespace
    {
        constexpr float FlagWidth = 34.0f;
        constexpr float FlagInset = 4.0f;
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawMeterRow(canvas::CanvasDrawingSession const& ds, float top, float height, float width, ArrangeDrawContext const& context)
    {
        auto const& meter = context.Doc->Meter;
        auto const& colors = *context.Colors;
        auto const flagTop = top + FlagInset;
        auto const flagHeight = std::max(1.0f, height - FlagInset * 2.0f);

        for (size_t i = 0; i < meter.size(); ++i)
        {
            auto const dragged = i == context.DraggedMeter;
            auto const tick = dragged ? context.DraggedMeterTick : meter[i].Tick;
            auto const x = std::floor(static_cast<float>(context.View.XAtTick(tick)));

            if (x + FlagWidth < 0 || x > width)
            {
                continue;
            }

            auto const selected = i == context.SelectedMeter || dragged;

            // A line down from the flag to the bar line, and the flag beside it.
            ds.FillRectangle(x, top + 1.0f, 1.5f, height - 2.0f, selected ? colors.SelectionStroke : colors.TempoLabelStroke);
            ds.FillRoundedRectangle(x + 1.5f, flagTop, FlagWidth, flagHeight, 3.0f, 3.0f, WithAlpha(colors.Accent, colors.Light ? 0.16 : 0.24));

            if (selected)
            {
                ds.DrawRoundedRectangle(x + 2.0f, flagTop + 0.5f, FlagWidth - 1.0f, flagHeight - 1.0f, 3.0f, 3.0f, colors.SelectionStroke, 1.5f);
            }

            ds.DrawText(winrt::hstring{ std::format(L"{}/{}", meter[i].Numerator, meter[i].Denominator) },
                x + 6.0f, flagTop, FlagWidth - 6.0f, flagHeight, colors.Text, m_tagText);
        }
    }
}