// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The lyrics row (design section 23): each syllable where it's sung, or each line where it starts
// when the syllables are too close together to read.

#include "pch.h"
#include "ArrangeRenderer.h"
#include "Lyrics.h"

namespace midisequencer
{
    namespace
    {
        // About how wide a character of the row's text is, so the row can tell whether a line's
        // syllables fit without measuring each one.
        constexpr float AverageCharacterWidth = 6.0f;
        constexpr float SyllableGap = 4.0f;
        constexpr float TextInset = 2.0f;

        // Narrower than this, a line shows only its marks.
        constexpr float MinimumLineWidth = 14.0f;

        // Sheet music's marks: a hyphen when the word goes on, a dash for a held syllable, and a
        // slash where a line ends (two for a verse), as karaoke files write them.
        std::wstring SyllableLabel(_In_ Lyric const& syllable)
        {
            std::wstring label = syllable.Text.empty() ? std::wstring{ L"\u2014" } : syllable.Text;

            switch (syllable.End)
            {
            case LyricEnd::None:
                if (!syllable.Text.empty())
                {
                    label += L'-';
                }
                break;

            case LyricEnd::Line:
                label += L" /";
                break;

            case LyricEnd::Paragraph:
                label += L" //";
                break;

            default:
                break;
            }

            return label;
        }

        float LabelWidth(_In_ std::wstring const& label) noexcept
        {
            return AverageCharacterWidth * static_cast<float>(label.size());
        }
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawLyricsRow(canvas::CanvasDrawingSession const& ds, float top, float height, float width, ArrangeDrawContext const& context)
    {
        auto const& doc = *context.Doc;
        auto const& colors = *context.Colors;

        if (!doc.Lyrics || doc.Lyrics->Syllables.empty())
        {
            ds.DrawText(context.EmptyLyricsHint, 12.0f, top, std::max(1.0f, width - 24.0f), height - 1.0f, colors.Text3, m_laneHint);
            return;
        }

        auto const& syllables = doc.Lyrics->Syllables;
        auto const lines = LyricLines(syllables);
        auto const pixelsPerTick = std::max(1.0e-9, context.View.PixelsPerTick);
        auto const leftTick = static_cast<int64_t>(std::max(0.0, context.View.ScrollX / pixelsPerTick));

        // The syllable being sung: the last one at or before the playhead.
        auto const after = std::upper_bound(syllables.begin(), syllables.end(), context.NowTick,
            [](int64_t tick, Lyric const& syllable) { return tick < syllable.Tick; });
        auto const current = after == syllables.begin() ? SIZE_MAX : static_cast<size_t>(after - syllables.begin()) - 1;

        auto const markTop = top + height - 7.0f;
        auto const textTop = top + 3.0f;
        auto const textHeight = height - 10.0f;

        for (auto index = LyricLineAtTick(lines, leftTick); index < lines.size(); ++index)
        {
            auto const& line = lines[index];
            auto const lineStart = static_cast<float>(context.View.XAtTick(line.StartTick));

            if (lineStart > width)
            {
                break;
            }

            auto const lineEnd = static_cast<float>(context.View.XAtTick(index + 1 < lines.size() ? lines[index + 1].StartTick : line.EndTick));
            auto const sungHere = current != SIZE_MAX && current >= line.First && current < line.First + line.Count;

            // Each syllable fits before the next one, or the line shows as a whole.
            auto roomy = true;

            for (size_t i = 0; i < line.Count && roomy; ++i)
            {
                auto const& syllable = syllables[line.First + i];
                auto const x = static_cast<float>(context.View.XAtTick(syllable.Tick));
                auto const next = i + 1 < line.Count ? static_cast<float>(context.View.XAtTick(syllables[line.First + i + 1].Tick)) : lineEnd;

                roomy = next - x >= LabelWidth(SyllableLabel(syllable)) + SyllableGap;
            }

            for (size_t i = 0; i < line.Count; ++i)
            {
                auto const& syllable = syllables[line.First + i];
                auto const x = static_cast<float>(context.View.XAtTick(syllable.Tick));

                if (x < -1.0f || x > width)
                {
                    continue;
                }

                ds.FillRectangle(x, markTop, 1.0f, 5.0f, colors.Text3);

                if (roomy)
                {
                    auto const label = SyllableLabel(syllable);
                    ds.DrawText(winrt::hstring{ label }, x + TextInset, textTop, LabelWidth(label) + 12.0f, textHeight,
                        line.First + i == current ? colors.Accent : colors.Text2, m_tagText);
                }
            }

            if (roomy || lineEnd - lineStart < MinimumLineWidth)
            {
                continue;
            }

            // The line's words, ending with an ellipsis where the next line starts. The last line
            // has nothing after it, so it runs on.
            auto const [sung, rest] = LyricLineText(syllables, line, INT64_MIN);
            auto const textWidth = index + 1 < lines.size() ? lineEnd - lineStart - SyllableGap : std::max(lineEnd - lineStart, width - lineStart);
            ds.DrawText(winrt::hstring{ sung + rest }, lineStart + TextInset, textTop, std::max(1.0f, textWidth - TextInset), textHeight,
                sungHere ? colors.Accent : colors.Text2, m_tagText);
        }
    }
}
