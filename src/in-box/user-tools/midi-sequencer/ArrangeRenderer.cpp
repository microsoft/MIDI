// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ArrangeRenderer.h"

using winrt::Windows::Foundation::Rect;

namespace midisequencer
{
    namespace
    {
        constexpr wchar_t TextFont[] = L"Segoe UI Variable Text";
        constexpr wchar_t GlyphFont[] = L"Segoe Fluent Icons";

        // Clip blocks sit 4 pixels inside their row, with a 14 pixel caption (seq.css .tclip).
        constexpr float ClipInset = 4.0f;
        constexpr float CaptionHeight = 14.0f;

        // A preview never draws more than this many notes in one block, so a dense recording
        // zoomed all the way out still draws quickly.
        constexpr size_t MaximumPreviewNotes = 6000;

        // A clip lane is drawn once for each pass of its clip that's in view. Past this many, the
        // passes are narrower than a pixel.
        constexpr size_t MaximumDrawnPasses = 4096;

        canvasText::CanvasTextFormat MakeFormat(
            wchar_t const* family,
            float size,
            uint16_t weight,
            canvasText::CanvasVerticalAlignment vertical = canvasText::CanvasVerticalAlignment::Center,
            canvasText::CanvasHorizontalAlignment horizontal = canvasText::CanvasHorizontalAlignment::Left)
        {
            canvasText::CanvasTextFormat format{};
            format.FontFamily(family);
            format.FontSize(size);
            format.FontWeight(winrt::Windows::UI::Text::FontWeight{ weight });
            format.WordWrapping(canvasText::CanvasWordWrapping::NoWrap);
            format.TrimmingGranularity(canvasText::CanvasTextTrimmingGranularity::Character);
            format.TrimmingSign(canvasText::CanvasTrimmingSign::Ellipsis);
            format.VerticalAlignment(vertical);
            format.HorizontalAlignment(horizontal);
            return format;
        }

        canvasGeometry::CanvasStrokeStyle DashedStroke(float dash, float gap)
        {
            canvasGeometry::CanvasStrokeStyle style{};
            style.CustomDashStyle({ dash, gap });
            return style;
        }

        Color TrackColor(uint32_t rgb) noexcept
        {
            return FromRgb(rgb & 0xFFFFFF);
        }

        // WCAG relative luminance of an sRGB color.
        double RelativeLuminance(uint32_t rgb) noexcept
        {
            auto const channel = [](uint32_t value)
            {
                auto const c = static_cast<double>(value & 0xFF) / 255.0;
                return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
            };

            return 0.2126 * channel(rgb >> 16) + 0.7152 * channel(rgb >> 8) + 0.0722 * channel(rgb);
        }

        // A tag's label has dark text (seq.css .tag). A color from another app can be too dark or
        // too strong for it, and then the text is black or white, whichever reads better.
        uint32_t TagTextColor(uint32_t background) noexcept
        {
            constexpr uint32_t Dark = 0x1A1A1A;
            auto const light = RelativeLuminance(background) + 0.05;

            if (light / (RelativeLuminance(Dark) + 0.05) >= 4.5)
            {
                return Dark;
            }

            return light / 0.05 >= 1.05 / light ? 0x000000 : 0xFFFFFF;
        }

        Color ClipColor(Palette const& colors, Clip const& clip, Track const& track) noexcept
        {
            return ThemedTrackColor(colors, TrackColor(clip.Color.value_or(track.Color)));
        }

        // CSS filter: saturate(.1) brightness(.75) in the dark theme, brightness(1.05) in the light.
        Color Muted(Palette const& colors, Color color, bool muted) noexcept
        {
            return muted ? Desaturate(color, 0.1, colors.Light ? 1.05 : 0.75) : color;
        }

        struct GridLine
        {
            float X{ 0 };
            bool Bar{ false };
            int64_t BarNumber{ 0 };

            // Far out, only every fourth bar is drawn strong; close in, beats are numbered and
            // sixteenths drawn (design section 31).
            bool Faint{ false };
            bool Step{ false };
            int64_t Beat{ 0 };
        };

        // Bar and beat lines across the visible width. Beats are left out when they'd be closer
        // than six pixels, and bars thinned the same way, so zooming out never draws a smear.
        std::vector<GridLine> VisibleGridLines(ArrangeDrawContext const& context, float width)
        {
            std::vector<GridLine> lines{};

            if (context.Doc == nullptr)
            {
                return lines;
            }

            auto const& meter = context.Doc->Meter;
            auto const& view = context.View;
            auto const firstTick = std::max<int64_t>(0, view.TickAtX(0));
            auto bar = BarPositionAtTick(meter, firstTick).Bar;

            auto const barPixels = static_cast<double>(TicksPerBar(meter.front())) * view.PixelsPerTick;
            int64_t barEvery{ 1 };

            while (barPixels * static_cast<double>(barEvery) < 6.0 && barEvery < 4096)
            {
                barEvery *= 2;
            }

            bar = ((bar - 1) / barEvery) * barEvery + 1;

            for (int guard = 0; guard < 20000; ++guard)
            {
                auto const tick = TickAtBar(meter, bar);
                auto const x = static_cast<float>(view.XAtTick(tick));

                if (x > width)
                {
                    break;
                }

                auto const& current = MeterAtTick(meter, tick);
                auto const beatTicks = TicksPerBeat(current);
                auto const beats = current.Numerator;
                auto const beatPixels = static_cast<double>(beatTicks) * view.PixelsPerTick;
                auto const showBeats = barEvery == 1 && beatPixels >= 6.0;

                lines.push_back(GridLine{ x, true, bar, !showBeats && (bar - 1) % (4 * barEvery) != 0 });

                if (showBeats)
                {
                    auto const steps = beatPixels / 4.0 >= 8.0 ? 4 : 1;

                    for (int64_t beat = 0; beat < beats; ++beat)
                    {
                        auto const beatTick = tick + beat * beatTicks;

                        if (beat > 0)
                        {
                            lines.push_back(GridLine{ static_cast<float>(view.XAtTick(beatTick)), false, 0, false, false, beat + 1 });
                        }

                        for (int64_t step = 1; step < steps; ++step)
                        {
                            lines.push_back(GridLine{ static_cast<float>(view.XAtTick(beatTick + step * beatTicks / 4)), false, 0, false, true, 0 });
                        }
                    }
                }

                bar += barEvery;
            }

            return lines;
        }

        std::wstring TempoText(double bpm)
        {
            auto const rounded = std::round(bpm);

            if (std::abs(bpm - rounded) < 0.005)
            {
                return std::format(L"{}", static_cast<int64_t>(rounded));
            }

            return std::format(L"{:.2f}", bpm);
        }

        float MeasureWidth(canvas::CanvasDrawingSession const& ds, winrt::hstring const& text, canvasText::CanvasTextFormat const& format)
        {
            canvasText::CanvasTextLayout layout{ ds, text, format, 10000.0f, 100.0f };
            return static_cast<float>(layout.LayoutBounds().Width);
        }

        std::unordered_map<std::wstring, size_t> CountUses(Sequence const& sequence)
        {
            std::unordered_map<std::wstring, size_t> uses{};

            ForEachTrack(sequence, [&uses](Track const& track, size_t)
            {
                for (auto const& placement : track.Timeline)
                {
                    ++uses[placement.ClipId];
                }

                for (auto const& slot : track.Slots)
                {
                    if (!slot.empty())
                    {
                        ++uses[slot];
                    }
                }

                return true;
            });

            return uses;
        }

        void CollectPlacements(Sequence const& sequence, std::vector<Track> const& tracks, std::vector<std::pair<int64_t, int64_t>>& spans)
        {
            for (auto const& track : tracks)
            {
                for (auto const& placement : track.Timeline)
                {
                    if (auto const clip = FindClip(sequence, placement.ClipId); clip != nullptr)
                    {
                        auto const length = placement.Length > 0 ? placement.Length : clip->Length;
                        spans.emplace_back(placement.Tick, placement.Tick + length);
                    }
                }

                CollectPlacements(sequence, track.Children, spans);
            }
        }

        thread_local std::unordered_map<std::wstring, size_t> t_useCounts{};
        thread_local std::vector<GridLine> t_gridLines{};
    }

    _Use_decl_annotations_
    std::wstring PlacementCaption(Clip const& clip, Placement const& placement)
    {
        auto const length = placement.Length > 0 ? placement.Length : clip.Length;

        if (clip.Loop && clip.Length > 0 && length > clip.Length)
        {
            auto const passes = (length + clip.Length - 1) / clip.Length;
            return std::format(L"{} \u00D7{}", clip.Name, passes);
        }

        return clip.Name;
    }

    namespace
    {
        constexpr float LaneRowPadding = 6.0f;
        constexpr double LaneTop = 4294967295.0;

        // The tempo row's line sits under its labels: 23 pixels from the top, 6 from the bottom.
        constexpr float TempoTopPadding = TempoTagBandHeight;
        constexpr float TempoBottomPadding = 6.0f;
    }

    _Use_decl_annotations_
    float LaneValueY(uint32_t value, float top, float height) noexcept
    {
        auto const span = std::max(1.0f, height - LaneRowPadding * 2.0f);
        return top + LaneRowPadding + static_cast<float>((1.0 - static_cast<double>(value) / LaneTop) * span);
    }

    _Use_decl_annotations_
    uint32_t LaneValueAtY(float y, float top, float height) noexcept
    {
        auto const span = std::max(1.0f, height - LaneRowPadding * 2.0f);
        auto const fraction = std::clamp(1.0 - static_cast<double>(y - top - LaneRowPadding) / span, 0.0, 1.0);
        return static_cast<uint32_t>(std::llround(fraction * LaneTop));
    }

    _Use_decl_annotations_
    TempoRange TempoRowRange(std::vector<TempoPoint> const& tempo) noexcept
    {
        if (tempo.empty())
        {
            return TempoRange{};
        }

        auto lowest = tempo.front().BeatsPerMinute;
        auto highest = lowest;

        for (auto const& point : tempo)
        {
            lowest = std::min(lowest, point.BeatsPerMinute);
            highest = std::max(highest, point.BeatsPerMinute);
        }

        auto const low = lowest - 6.0;
        return TempoRange{ low, std::max(highest + 4.0, low + 1.0) };
    }

    _Use_decl_annotations_
    float TempoY(double beatsPerMinute, TempoRange const& range, float top, float height) noexcept
    {
        auto const span = std::max(1.0f, height - TempoTopPadding - TempoBottomPadding);
        return top + TempoTopPadding + static_cast<float>((1.0 - (beatsPerMinute - range.Low) / (range.High - range.Low)) * span);
    }

    _Use_decl_annotations_
    double TempoAtY(float y, TempoRange const& range, float top, float height) noexcept
    {
        auto const span = std::max(1.0f, height - TempoTopPadding - TempoBottomPadding);
        return range.Low + (1.0 - static_cast<double>(y - top - TempoTopPadding) / span) * (range.High - range.Low);
    }

    _Use_decl_annotations_
    size_t ArrangeRenderer::UseCount(std::wstring const& clipId) noexcept
    {
        auto const found = t_useCounts.find(clipId);
        return found != t_useCounts.end() ? found->second : 0;
    }

    void ArrangeRenderer::EnsureFormats()
    {
        if (m_caption != nullptr)
        {
            return;
        }

        m_caption = MakeFormat(TextFont, 10.0f, 400);
        m_captionGlyph = MakeFormat(GlyphFont, 8.0f, 400);
        m_rulerNumber = MakeFormat(TextFont, 10.0f, 400, canvasText::CanvasVerticalAlignment::Top);
        m_tagText = MakeFormat(TextFont, 10.0f, 600);
        m_rubyText = MakeFormat(TextFont, 9.5f, 400);
        m_cellName = MakeFormat(TextFont, 10.5f, 400);
        m_cellGlyph = MakeFormat(GlyphFont, 8.0f, 400);
        m_slotGlyph = MakeFormat(GlyphFont, 8.0f, 400, canvasText::CanvasVerticalAlignment::Center, canvasText::CanvasHorizontalAlignment::Center);
        m_kindGlyph = MakeFormat(GlyphFont, 10.0f, 400, canvasText::CanvasVerticalAlignment::Center, canvasText::CanvasHorizontalAlignment::Center);
        m_badge = MakeFormat(TextFont, 9.5f, 400);
        m_tempoLabel = MakeFormat(TextFont, 10.0f, 400);
        m_meterLabel = MakeFormat(TextFont, 9.5f, 600);
        m_laneHint = MakeFormat(TextFont, 11.0f, 400);
        m_overlayText = MakeFormat(TextFont, 11.0f, 400);
        m_overlayButton = MakeFormat(TextFont, 12.0f, 400, canvasText::CanvasVerticalAlignment::Center, canvasText::CanvasHorizontalAlignment::Center);
    }

    _Use_decl_annotations_
    std::pair<uint8_t, uint8_t> ArrangeRenderer::PitchRange(Clip const& clip, std::vector<Note> const& notes)
    {
        auto const compute = [&notes]()
        {
            if (notes.empty())
            {
                return std::pair<uint8_t, uint8_t>{ static_cast<uint8_t>(56), static_cast<uint8_t>(68) };
            }

            uint8_t low{ 127 };
            uint8_t high{ 0 };

            for (auto const& note : notes)
            {
                low = std::min(low, note.Number);
                high = std::max(high, note.Number);
            }

            if (high - low < 10)
            {
                low = static_cast<uint8_t>(std::max(0, low - 4));
                high = static_cast<uint8_t>(std::min(127, high + 4));
            }

            return std::pair<uint8_t, uint8_t>{ low, high };
        };

        // A take still being recorded changes as it goes, so it isn't kept.
        if (&notes != &clip.Notes)
        {
            return compute();
        }

        auto found = m_pitchRanges.find(clip.Id);

        if (found != m_pitchRanges.end())
        {
            return found->second;
        }

        auto const range = compute();
        m_pitchRanges.emplace(clip.Id, range);
        return range;
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawRuler(canvas::CanvasDrawingSession const& ds, float width, float height, ArrangeDrawContext const& context)
    {
        if (context.Doc == nullptr || context.Colors == nullptr)
        {
            return;
        }

        EnsureFormats();

        auto const& colors = *context.Colors;
        auto const lines = VisibleGridLines(context, width);

        // A theme can set the bar numbers' color and font.
        auto numberColor = colors.Text3;
        auto numberFormat = m_rulerNumber;
        auto lining = false;

        // A font with old-style figures (Vellum's Candara) is asked for lining ones.
        auto const drawNumber = [&](std::wstring const& text, float x, Color color)
        {
            if (!lining)
            {
                ds.DrawText(winrt::hstring{ text }, x, 3.0f, 60.0f, 14.0f, color, numberFormat);
                return;
            }

            canvasText::CanvasTextLayout layout{ ds, winrt::hstring{ text }, numberFormat, 60.0f, 14.0f };
            canvasText::CanvasTypography typography{};
            typography.AddFeature(canvasText::CanvasTypographyFeatureName::LiningFigures, 1);
            typography.AddFeature(canvasText::CanvasTypographyFeatureName::TabularFigures, 1);
            layout.SetTypography(0, static_cast<int32_t>(text.size()), typography);
            ds.DrawTextLayout(layout, x, 3.0f, color);
        };

        if (colors.Theme != nullptr && colors.Theme->HasPart(L"ruler"))
        {
            auto const ruler = colors.Theme->Part(L"ruler");
            numberColor = ToUiColor(ruler->ColorOr(L"number-color", ToThemeColor(numberColor)));

            if (auto const font = ruler->Word(L"number-font", {}); !font.empty())
            {
                numberFormat = ThemedFormat(colors.Theme->Font(font, TextFont), 10.0f, 400);
                numberFormat.VerticalAlignment(canvasText::CanvasVerticalAlignment::Top);
            }

            lining = ruler->Word(L"numerals", {}) == L"lining";
        }

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
        ds.FillRectangle(0, 0, width, height, colors.RulerBackground);

        // Bar numbers spread out when the bars get narrow, so they never run into each other.
        auto const barPixels = static_cast<double>(TicksPerBar(context.Doc->Meter.front())) * context.View.PixelsPerTick;
        int64_t labelEvery{ 1 };

        while (barPixels * static_cast<double>(labelEvery) < 24.0 && labelEvery < 4096)
        {
            labelEvery *= 2;
        }

        int64_t currentBar{ 1 };

        for (auto const& line : lines)
        {
            auto const x = std::floor(line.X);

            if (line.Bar)
            {
                currentBar = line.BarNumber;
                ds.FillRectangle(x, 0, 1, height, colors.RulerBarLine);

                if ((line.BarNumber - 1) % labelEvery == 0)
                {
                    drawNumber(std::to_wstring(line.BarNumber), x + 3.0f, numberColor);
                }
            }
            else if (line.Step)
            {
                ds.FillRectangle(x, height - 5.0f, 1, 5.0f, colors.RulerBeatLine);
            }
            else
            {
                ds.FillRectangle(x, 20.0f, 1, height - 20.0f, colors.RulerBeatLine);

                // Close in, the beats are numbered too.
                if (line.Beat > 0 && barPixels / 4.0 >= 40.0)
                {
                    drawNumber(std::format(L"{}.{}", currentBar, line.Beat), x + 3.0f, WithAlpha(numberColor, numberColor.A / 255.0 * 0.6));
                }
            }
        }

        if (context.LoopEnd > context.LoopStart)
        {
            auto const x0 = static_cast<float>(context.View.XAtTick(context.LoopStart));
            auto const x1 = static_cast<float>(context.View.XAtTick(context.LoopEnd));

            if (x1 > 0 && x0 < width && !DrawThemedLoop(ds, x0, x1, height, context))
            {
                ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

                auto const fill = context.LoopEnabled ? colors.LoopFill : WithAlpha(colors.LoopFill, colors.LoopFill.A / 255.0 * 0.4);
                auto const stroke = context.LoopEnabled ? colors.LoopStroke : WithAlpha(colors.LoopStroke, colors.LoopStroke.A / 255.0 * 0.4);

                ds.FillRoundedRectangle(x0, height - 9.0f, x1 - x0, 11.0f, 2.0f, 2.0f, fill);
                ds.DrawRoundedRectangle(x0 + 0.5f, height - 8.5f, x1 - x0 - 1.0f, 11.0f, 2.0f, 2.0f, stroke, 1.0f);
                ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
            }
        }

        ds.FillRectangle(0, height - 1.0f, width, 1.0f, colors.Divider);
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawGrid(
        canvas::CanvasDrawingSession const& ds,
        float top,
        float height,
        float width,
        ArrangeDrawContext const& context,
        Color barColor,
        Color beatColor)
    {
        UNREFERENCED_PARAMETER(width);

        // An engraved grid has a light line beside each bar line.
        auto const light = context.Colors != nullptr ? context.Colors->BarLineLight : Color{};

        auto const stepColor = WithAlpha(beatColor, beatColor.A / 255.0 * 0.5);

        for (auto const& line : t_gridLines)
        {
            ds.FillRectangle(std::floor(line.X), top, 1.0f, height, line.Bar && !line.Faint ? barColor : line.Step ? stepColor : beatColor);

            if (line.Bar && !line.Faint && light.A > 0)
            {
                ds.FillRectangle(std::floor(line.X) + 1.0f, top, 1.0f, height, light);
            }
        }
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawLanes(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        std::vector<ArrangeRow> const& rows,
        double scrollY,
        bool fillBelow,
        ArrangeDrawContext const& context)
    {
        if (context.Doc == nullptr || context.Colors == nullptr)
        {
            return;
        }

        EnsureFormats();

        auto const& colors = *context.Colors;
        auto const& doc = *context.Doc;

        t_gridLines = VisibleGridLines(context, width);
        t_useCounts = CountUses(doc);

        // A theme's field under every lane, which the lanes are drawn inside.
        auto field = DrawField(ds, width, height, !fillBelow, context);

        float bottom{ 0 };

        for (auto const& row : rows)
        {
            auto const top = static_cast<float>(row.Top - scrollY);
            auto const rowHeight = static_cast<float>(row.Height);
            bottom = top + rowHeight;

            if (top > height)
            {
                break;
            }

            if (bottom < 0)
            {
                continue;
            }

            ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);

            auto const background = row.Kind == ArrangeRowKind::Folder ? colors.FolderLaneBackground : colors.LaneBackground;
            ds.FillRectangle(0, top, width, rowHeight, background);
            DrawGrid(ds, top, rowHeight, width, context, colors.BarLine, colors.BeatLine);
            DrawRowSeam(ds, bottom, width, colors);

            ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

            switch (row.Kind)
            {
            case ArrangeRowKind::Tempo:
                DrawTempoLane(ds, top, rowHeight, width, context);
                break;

            case ArrangeRowKind::Track:
                if (auto const track = FindTrack(doc, row.TrackId); track != nullptr)
                {
                    DrawTrackLane(ds, *track, row, top, width, context);
                }
                break;

            case ArrangeRowKind::Folder:
                if (auto const folder = FindTrack(doc, row.TrackId); folder != nullptr)
                {
                    DrawFolderLane(ds, *folder, top, rowHeight, width, context);
                }
                break;

            case ArrangeRowKind::Lane:
                if (auto const track = FindTrack(doc, row.TrackId); track != nullptr)
                {
                    DrawAutomationRow(ds, *track, row, top, width, context);
                }
                break;

            case ArrangeRowKind::Lyrics:
                DrawLyricsRow(ds, top, rowHeight, width, context);
                break;

            default:
                break;
            }
        }

        // Below the last lane, the grid goes on; on a theme's field the space stays plain.
        if (fillBelow && bottom < height && field == nullptr)
        {
            auto const top = std::max(0.0f, bottom);

            ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
            ds.FillRectangle(0, top, width, height - top, colors.LaneBackground);
            DrawGrid(ds, top, height - top, width, context, colors.BarLine, colors.BeatLine);
        }

        if (field != nullptr)
        {
            field.Close();
        }

        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawTempoLane(canvas::CanvasDrawingSession const& ds, float top, float height, float width, ArrangeDrawContext const& context)
    {
        auto const& colors = *context.Colors;
        auto const& tempo = context.Doc->Tempo;

        if (tempo.empty())
        {
            return;
        }

        auto const range = context.TempoScale.value_or(TempoRowRange(tempo));
        auto const y = [&](double bpm) { return TempoY(bpm, range, top, height); };
        auto const x = [&](int64_t tick) { return static_cast<float>(context.View.XAtTick(tick)); };

        canvasGeometry::CanvasPathBuilder path{ ds };
        path.BeginFigure(x(tempo.front().Tick), y(tempo.front().BeatsPerMinute));

        for (size_t i = 0; i + 1 < tempo.size(); ++i)
        {
            auto const& point = tempo[i];
            auto const& next = tempo[i + 1];

            if (point.RampToNext)
            {
                path.AddLine(x(next.Tick), y(next.BeatsPerMinute));
            }
            else
            {
                path.AddLine(x(next.Tick), y(point.BeatsPerMinute));
                path.AddLine(x(next.Tick), y(next.BeatsPerMinute));
            }
        }

        path.AddLine(std::max(width + 10.0f, x(tempo.back().Tick) + 1.0f), y(tempo.back().BeatsPerMinute));
        path.EndFigure(canvasGeometry::CanvasFigureLoop::Open);

        ds.DrawGeometry(canvasGeometry::CanvasGeometry::CreatePath(path), colors.TempoLine, 1.5f);

        // A label where the tempo changes, and at the start.
        for (size_t i = 0; i < tempo.size(); ++i)
        {
            if (i > 0 && std::abs(tempo[i].BeatsPerMinute - tempo[i - 1].BeatsPerMinute) < 0.0001)
            {
                continue;
            }

            auto const px = x(tempo[i].Tick);
            auto const py = y(tempo[i].BeatsPerMinute);

            if (px > width || px < -60.0f)
            {
                continue;
            }

            auto const text = winrt::hstring{ TempoText(tempo[i].BeatsPerMinute) };
            auto const labelWidth = static_cast<float>(text.size()) * 6.0f + 8.0f;

            ds.FillRoundedRectangle(px + 6.0f, py - 7.0f, labelWidth, 14.0f, 3.0f, 3.0f, colors.TempoLabelFill);
            ds.DrawRoundedRectangle(px + 6.0f, py - 7.0f, labelWidth, 14.0f, 3.0f, 3.0f, colors.TempoLabelStroke, 0.8f);
            ds.DrawText(text, px + 10.0f, py - 7.0f, labelWidth, 14.0f, colors.TempoLabelText, m_tempoLabel);
        }

        // A handle on each change, so it can be dragged.
        for (size_t i = 0; i < tempo.size(); ++i)
        {
            auto const px = x(tempo[i].Tick);

            if (px < -6.0f || px > width + 6.0f)
            {
                continue;
            }

            auto const selected = context.SelectedTempoPoint == i;
            auto const radius = selected ? 4.0f : 2.5f;
            auto const py = y(tempo[i].BeatsPerMinute);

            ds.FillCircle(px, py, radius, selected ? colors.SelectionStroke : colors.HandleFill);
            ds.DrawCircle(px, py, radius, selected ? colors.SelectionStroke : colors.TempoLine, 1.2f);
        }

        // Meter changes after the first, along the bottom.
        auto const& meter = context.Doc->Meter;

        for (size_t i = 1; i < meter.size(); ++i)
        {
            auto const mx = x(meter[i].Tick);

            if (mx < -40.0f || mx > width)
            {
                continue;
            }

            ds.FillRectangle(std::floor(mx), top + height - 15.0f, 1.0f, 14.0f, colors.TempoLabelStroke);
            ds.DrawText(winrt::hstring{ std::format(L"{}/{}", meter[i].Numerator, meter[i].Denominator) },
                mx + 3.0f, top + height - 15.0f, 40.0f, 13.0f, colors.Text3, m_meterLabel);
        }

        for (auto const& tag : context.Doc->Tags)
        {
            DrawTag(ds, tag, x(tag.Tick), top, height, true, context);
        }
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawTrackLane(
        canvas::CanvasDrawingSession const& ds,
        Track const& track,
        ArrangeRow const& row,
        float top,
        float width,
        ArrangeDrawContext const& context)
    {
        auto const& colors = *context.Colors;
        auto const height = static_cast<float>(row.Height);
        auto const muted = track.Muted || row.InsideMutedFolder;
        auto const drag = context.ClipDrag.has_value() ? &*context.ClipDrag : nullptr;

        for (size_t i = 0; i < track.Timeline.size(); ++i)
        {
            auto const selected = context.SelectedTrackId == track.Id && context.SelectedPlacement == i;

            // A clip being moved stays faded where it was until it lands.
            if (drag != nullptr && !drag->Copy && drag->FromTrackId == track.Id && drag->FromPlacement == i)
            {
                auto faded = ds.CreateLayer(0.35f);
                DrawClipBlock(ds, track, track.Timeline[i], false, muted, top, height, width, context);
                faded.Close();
                continue;
            }

            DrawClipBlock(ds, track, track.Timeline[i], selected, muted, top, height, width, context);
        }

        if (drag != nullptr && drag->ToTrackId == track.Id && drag->ToTick >= 0)
        {
            Placement ghost{};
            ghost.ClipId = drag->ClipId;
            ghost.Tick = drag->ToTick;
            ghost.Length = drag->Length;

            auto layer = ds.CreateLayer(0.8f);
            DrawClipBlock(ds, track, ghost, true, false, top, height, width, context);
            layer.Close();
        }

        // A take being recorded grows from where it started to the playhead.
        if (context.RecordingTrackId == track.Id && context.NowTick > context.RecordingStartTick)
        {
            auto const x0 = static_cast<float>(context.View.XAtTick(context.RecordingStartTick)) + 1.0f;
            auto const x1 = static_cast<float>(context.View.XAtTick(context.NowTick));

            if (x1 > x0 && x1 > 0 && x0 < width)
            {
                auto const inset = height < static_cast<float>(OneLineRowHeight) ? 2.0f : ClipInset;
                auto const y0 = top + inset;
                auto const h = height - inset * 2;
                auto const fill = WithAlpha(colors.Record, 0.30);
                auto const geometry = canvasGeometry::CanvasGeometry::CreateRoundedRectangle(ds, x0, y0, x1 - x0, h, 4.0f, 4.0f);

                ds.FillGeometry(geometry, fill);

                {
                    auto layer = ds.CreateLayer(1.0f, geometry);
                    ds.FillRectangle(x0, y0, x1 - x0, CaptionHeight, WithAlpha(colors.Record, 0.75));
                    ds.DrawText(context.RecordingCaption, x0 + 5.0f, y0, std::max(1.0f, x1 - x0 - 9.0f), CaptionHeight, Rgba(255, 255, 255), m_caption);

                    if (context.RecordingNotes != nullptr)
                    {
                        Clip take{};
                        take.Length = std::max<int64_t>(1, context.NowTick - context.RecordingStartTick);
                        take.Loop = false;

                        Rect const area{ x0, y0 + CaptionHeight, x1 - x0, h - CaptionHeight };
                        DrawNotes(ds, take, *context.RecordingNotes, area, context.RecordingStartTick, context.NowTick,
                            context.View.PixelsPerTick, -context.View.ScrollX, colors.Record, false);
                    }

                    layer.Close();
                }

                ds.DrawRoundedRectangle(x0 + 0.5f, y0 + 0.5f, x1 - x0 - 1.0f, h - 1.0f, 3.5f, 3.5f, colors.Record, 1.0f);
            }
        }

        for (auto const& tag : track.Tags)
        {
            DrawTag(ds, tag, static_cast<float>(context.View.XAtTick(tag.Tick)), top, height, false, context);
        }

        // A track playing a launched clip, or stopped from the launcher, isn't following its
        // timeline, so the timeline is covered until Back to timeline.
        if (context.Launch != nullptr)
        {
            auto const found = context.Launch->find(track.Id);

            if (found != context.Launch->end() && found->second.Mode != TrackPlayMode::Timeline)
            {
                auto layer = ds.CreateLayer(1.0f, Rect{ 0, top, width, height - 1.0f });

                ds.FillRectangle(0, top, width, height - 1.0f, colors.OverlayStripeB);

                for (float stripe = -height; stripe < width + height; stripe += 17.0f)
                {
                    ds.DrawLine(stripe, top + height, stripe + height, top, colors.OverlayStripeA, 6.0f);
                }

                auto const& caption = found->second.Mode == TrackPlayMode::Stopped ? context.StoppedFromLauncher : context.PlayingLaunchedClip;
                auto const textWidth = MeasureWidth(ds, caption, m_overlayText);
                ds.DrawText(caption, 12.0f, top, textWidth + 4.0f, height - 1.0f, colors.Text2, m_overlayText);

                // The button is in the same place on every row, whichever caption it follows.
                m_overlayTextWidth = std::max(MeasureWidth(ds, context.PlayingLaunchedClip, m_overlayText), MeasureWidth(ds, context.StoppedFromLauncher, m_overlayText));
                m_overlayButtonWidth = MeasureWidth(ds, context.BackToTimeline, m_overlayButton) + 18.0f;

                auto const button = BackToTimelineButtonBounds(top, height);
                auto const buttonFill = colors.Light ? Rgba(255, 255, 255) : Rgba(40, 40, 40, 0.9);

                ds.FillRoundedRectangle(button, 4.0f, 4.0f, buttonFill);
                ds.DrawRoundedRectangle(button.X + 0.5f, button.Y + 0.5f, button.Width - 1.0f, button.Height - 1.0f, 3.5f, 3.5f, colors.StrokeStrong, 1.0f);
                ds.DrawText(context.BackToTimeline, button, colors.Text, m_overlayButton);

                layer.Close();
            }
        }
    }

    _Use_decl_annotations_
    Rect ArrangeRenderer::BackToTimelineButtonBounds(float rowTop, float rowHeight) const noexcept
    {
        auto const x = 12.0f + m_overlayTextWidth + 8.0f;
        auto const buttonHeight = std::clamp(rowHeight - 5.0f, 16.0f, 26.0f);
        return Rect{ x, rowTop + (rowHeight - 1.0f - buttonHeight) / 2.0f, std::max(60.0f, m_overlayButtonWidth), buttonHeight };
    }

    _Use_decl_annotations_
    Rect ArrangeRenderer::TagLabelBounds(Tag const& tag, float x, float top, float height, bool atTop)
    {
        if (tag.Text.empty())
        {
            return Rect{ x, top, 0, 0 };
        }

        EnsureFormats();

        canvasText::CanvasTextLayout layout{ canvas::CanvasDevice::GetSharedDevice(), winrt::hstring{ tag.Text }, m_tagText, 10000.0f, 100.0f };
        auto const textWidth = std::min(220.0f, static_cast<float>(layout.LayoutBounds().Width));
        auto const labelWidth = 12.0f + textWidth + 6.0f;
        auto const labelHeight = 16.0f;
        auto const y = atTop ? top + 3.0f : top + height - 5.0f - labelHeight;

        return Rect{ x, y, labelWidth, labelHeight };
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawFolderLane(canvas::CanvasDrawingSession const& ds, Track const& folder, float top, float height, float width, ArrangeDrawContext const& context)
    {
        auto const& colors = *context.Colors;

        // An outline of what the folder's tracks play, so the song's shape shows even when it's closed.
        std::vector<std::pair<int64_t, int64_t>> spans{};
        CollectPlacements(*context.Doc, folder.Children, spans);

        if (spans.empty())
        {
            return;
        }

        std::sort(spans.begin(), spans.end());

        std::vector<std::pair<int64_t, int64_t>> merged{};

        for (auto const& span : spans)
        {
            if (!merged.empty() && span.first <= merged.back().second)
            {
                merged.back().second = std::max(merged.back().second, span.second);
            }
            else
            {
                merged.push_back(span);
            }
        }

        auto const tc = ThemedTrackColor(colors, TrackColor(folder.Color));
        auto const fill = Muted(colors, SummaryFill(colors, tc), folder.Muted);
        auto const stroke = Muted(colors, SummaryStroke(colors, tc), folder.Muted);

        // A theme can draw the outline as one of its clips in their "summary" state.
        std::shared_ptr<themeengine::ResolvedStyle const> summary{};

        if (colors.Theme != nullptr && colors.Theme->HasPart(L"timeline.clip") && context.Creator != nullptr)
        {
            std::vector<std::wstring_view> states{ L"summary" };

            if (folder.Muted)
            {
                states.push_back(L"muted");
            }

            summary = colors.Theme->Part(L"timeline.clip", std::span<std::wstring_view const>{ states }, ToThemeColor(tc));
        }

        for (auto const& [start, end] : merged)
        {
            auto const x0 = static_cast<float>(context.View.XAtTick(start)) + 1.0f;
            auto const x1 = static_cast<float>(context.View.XAtTick(end)) - 1.0f;

            if (x1 < 0 || x0 > width || x1 - x0 < 1.0f)
            {
                continue;
            }

            if (summary != nullptr)
            {
                auto const inset = static_cast<float>(summary->Number(L"inset", 6.0));
                auto const drawX0 = std::max(x0, -16.0f);
                auto const drawX1 = std::min(x1, width + 16.0f);
                m_painter.Paint(ds, context.Creator, *summary, { drawX0, top + inset, drawX1 - drawX0, height - inset * 2.0f });
                continue;
            }

            ds.FillRoundedRectangle(x0, top + 6.0f, x1 - x0, height - 12.0f, 4.0f, 4.0f, fill);
            ds.DrawRoundedRectangle(x0 + 0.5f, top + 6.5f, x1 - x0 - 1.0f, height - 13.0f, 3.5f, 3.5f, stroke, 1.0f);
        }
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawLaneLine(
        canvas::CanvasDrawingSession const& ds,
        AutomationLane const& lane,
        int64_t offset,
        int64_t fromTick,
        int64_t toTick,
        float top,
        float height,
        float width,
        ArrangeDrawContext const& context,
        Color color,
        bool fill,
        bool dashed)
    {
        auto const& points = lane.Points;
        auto const& view = context.View;

        // Only what's on screen.
        auto const visibleFrom = std::max(fromTick, std::max<int64_t>(0, view.TickAtX(-2.0)));
        auto const visibleTo = std::min(toTick, view.TickAtX(static_cast<double>(width) + 2.0) + 1);

        if (points.empty() || visibleTo <= visibleFrom)
        {
            return;
        }

        auto const x = [&](int64_t tick) { return static_cast<float>(view.XAtTick(tick)); };
        auto const y = [&](uint32_t value) { return LaneValueY(value, top, height); };
        auto const valueAt = [&](int64_t tick) { return LaneValueAt(lane, tick - offset).value_or(0); };

        std::vector<std::pair<float, float>> line{};
        auto current = valueAt(visibleFrom);
        line.emplace_back(x(visibleFrom), y(current));

        auto index = static_cast<size_t>(std::distance(points.begin(), std::lower_bound(points.begin(), points.end(), visibleFrom - offset,
            [](LanePoint const& point, int64_t tick) { return point.Tick < tick; })));

        auto const firstHandle = index;

        for (; index < points.size() && points[index].Tick + offset < visibleTo; ++index)
        {
            auto const px = x(points[index].Tick + offset);

            // A ramp arrives at the point's value; a hold stays level up to it, then steps.
            if (index == 0 || !LaneRampsAfter(lane, index - 1))
            {
                line.emplace_back(px, y(current));
            }

            current = points[index].Value;
            line.emplace_back(px, y(current));
        }

        auto const lastHandle = index;
        line.emplace_back(x(visibleTo), y(valueAt(visibleTo - 1)));

        auto const build = [&](bool closed)
        {
            canvasGeometry::CanvasPathBuilder path{ ds };
            path.BeginFigure(line.front().first, line.front().second);

            for (size_t i = 1; i < line.size(); ++i)
            {
                path.AddLine(line[i].first, line[i].second);
            }

            if (closed)
            {
                path.AddLine(line.back().first, top + height - 1.0f);
                path.AddLine(line.front().first, top + height - 1.0f);
            }

            path.EndFigure(closed ? canvasGeometry::CanvasFigureLoop::Closed : canvasGeometry::CanvasFigureLoop::Open);
            return canvasGeometry::CanvasGeometry::CreatePath(path);
        };

        if (fill)
        {
            ds.FillGeometry(build(true), WithAlpha(color, 0.12));
        }

        if (dashed)
        {
            ds.DrawGeometry(build(false), WithAlpha(color, 0.85), 1.4f, DashedStroke(4.0f, 3.0f));
        }
        else
        {
            ds.DrawGeometry(build(false), color, 1.6f);
        }

        // Handles on the points, when they're far enough apart to aim at.
        if (!fill || lastHandle - firstHandle > 2000)
        {
            return;
        }

        auto const& colors = *context.Colors;
        auto lastX = -1000.0f;
        auto lastY = -1000.0f;

        for (auto i = firstHandle; i < lastHandle; ++i)
        {
            auto const selected = context.SelectedLanePoint == i;
            auto const radius = selected ? 4.0f : 3.0f;
            auto const px = x(points[i].Tick + offset);
            auto const py = y(points[i].Value);

            // Drawn points come thick and fast; a handle on top of the last one is noise.
            if (!selected && (px - lastX) * (px - lastX) + (py - lastY) * (py - lastY) < 49.0f)
            {
                continue;
            }

            lastX = px;
            lastY = py;

            ds.FillCircle(px, py, radius, selected ? colors.SelectionStroke : colors.HandleFill);
            ds.DrawCircle(px, py, radius, selected ? colors.SelectionStroke : color, 1.5f);
        }
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawAutomationRow(canvas::CanvasDrawingSession const& ds, Track const& track, ArrangeRow const& row, float top, float width, ArrangeDrawContext const& context)
    {
        auto const& colors = *context.Colors;
        auto const height = static_cast<float>(row.Height);
        auto const lane = std::find_if(track.Lanes.begin(), track.Lanes.end(), [&row](AutomationLane const& candidate) { return candidate.Id == row.LaneId; });

        if (lane == track.Lanes.end())
        {
            return;
        }

        if (lane->Points.empty())
        {
            ds.DrawText(context.EmptyLaneHint, 12.0f, top, std::max(1.0f, width - 24.0f), height - 1.0f, colors.Text3, m_laneHint);
            return;
        }

        auto const muted = track.Muted || row.InsideMutedFolder;
        auto const color = Muted(colors, TrackColor(track.Color), muted);
        auto const selected = context.SelectedLaneTrackId == track.Id && context.SelectedLaneId == lane->Id;

        auto selectedContext = context;

        if (!selected)
        {
            selectedContext.SelectedLanePoint = SIZE_MAX;
        }

        DrawLaneLine(ds, *lane, 0, 0, INT64_MAX, top, height, width, selectedContext, color, true, false);

        // Inside a clip that has its own lane for this, the clip's lane plays instead: the track's
        // line is shaded there, and the clip's is drawn over it each time the clip goes round.
        for (auto const& placement : track.Timeline)
        {
            auto const clip = FindClip(*context.Doc, placement.ClipId);

            if (clip == nullptr || clip->Length <= 0)
            {
                continue;
            }

            auto const own = std::find_if(clip->Lanes.begin(), clip->Lanes.end(), [&lane](AutomationLane const& candidate)
            {
                return !candidate.Points.empty() && SameLaneTarget(candidate, *lane);
            });

            if (own == clip->Lanes.end())
            {
                continue;
            }

            auto const length = placement.Length > 0 ? placement.Length : clip->Length;
            auto const end = placement.Tick + (clip->Loop ? length : std::min(length, clip->Length));
            auto const x0 = std::max(0.0f, static_cast<float>(context.View.XAtTick(placement.Tick)));
            auto const x1 = std::min(width, static_cast<float>(context.View.XAtTick(end)));

            if (x1 <= x0)
            {
                continue;
            }

            ds.FillRectangle(x0, top, x1 - x0, height - 1.0f, colors.OverlayStripeB);

            // Under the line, so the line stays readable where they cross.
            ds.DrawText(context.ClipLaneCaption, x0 + 4.0f, top + 2.0f, std::max(1.0f, x1 - x0 - 8.0f), 13.0f, colors.Text3, m_meterLabel);

            // From the first pass in view, and no more passes than the width can show.
            auto passStart = placement.Tick;

            if (auto const leftTick = static_cast<int64_t>(std::max(0.0, context.View.ScrollX / std::max(1.0e-9, context.View.PixelsPerTick))); leftTick > passStart)
            {
                passStart += ((leftTick - passStart) / clip->Length) * clip->Length;
            }

            for (size_t drawn = 0; passStart < end && context.View.XAtTick(passStart) <= width && drawn < MaximumDrawnPasses; passStart += clip->Length, ++drawn)
            {
                DrawLaneLine(ds, *own, passStart, passStart, std::min(end, passStart + clip->Length), top, height, width, context, color, false, true);
            }
        }
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawClipBlock(
        canvas::CanvasDrawingSession const& ds,
        Track const& track,
        Placement const& placement,
        bool selected,
        bool muted,
        float top,
        float height,
        float width,
        ArrangeDrawContext const& context)
    {
        auto const clip = FindClip(*context.Doc, placement.ClipId);

        if (clip == nullptr || clip->Length <= 0)
        {
            return;
        }

        if (DrawThemedClip(ds, track, *clip, placement, selected, muted, top, height, width, context))
        {
            return;
        }

        auto const& colors = *context.Colors;
        auto const length = placement.Length > 0 ? placement.Length : clip->Length;

        auto const x0 = static_cast<float>(context.View.XAtTick(placement.Tick)) + 1.0f;
        auto const x1 = std::max(x0 + 2.0f, static_cast<float>(context.View.XAtTick(placement.Tick + length)) - 1.0f);

        if (x1 < -4.0f || x0 > width + 4.0f)
        {
            return;
        }

        auto const inset = height < static_cast<float>(OneLineRowHeight) ? 2.0f : ClipInset;
        auto const y0 = top + inset;
        auto const h = height - inset * 2;
        auto const w = x1 - x0;

        auto const tc = ClipColor(colors, *clip, track);
        auto const fill = Muted(colors, ClipFill(colors, tc), muted);
        auto const stroke = Muted(colors, ClipStroke(colors, tc), muted);
        auto const caption = Muted(colors, ClipCaption(colors, tc), muted);
        auto const noteColor = Muted(colors, tc, muted);

        // Drawing a block hundreds of thousands of pixels wide is pointless: clamp it to the view.
        auto const drawX0 = std::max(x0, -8.0f);
        auto const drawX1 = std::min(x1, width + 8.0f);
        auto const roundLeft = drawX0 == x0;
        auto const roundRight = drawX1 == x1;

        auto const geometry = canvasGeometry::CanvasGeometry::CreateRoundedRectangle(ds, drawX0, y0, drawX1 - drawX0, h,
            roundLeft || roundRight ? 4.0f : 0.0f, roundLeft || roundRight ? 4.0f : 0.0f);

        ds.FillGeometry(geometry, fill);

        {
            auto layer = ds.CreateLayer(1.0f, geometry);

            ds.FillRectangle(drawX0, y0, drawX1 - drawX0, CaptionHeight, caption);

            auto textX = std::max(x0, 0.0f) + 5.0f;
            auto const captionColor = colors.Light ? FromRgb(0x1A1A1A) : Rgba(255, 255, 255);

            auto const uses = t_useCounts.find(clip->Id);

            if (uses != t_useCounts.end() && uses->second > 1)
            {
                ds.DrawText(L"\uE71B", textX, y0, 10.0f, CaptionHeight, WithAlpha(captionColor, 0.85), m_captionGlyph);
                textX += 12.0f;
            }

            auto const available = x1 - textX - 4.0f;

            if (available > 4.0f)
            {
                std::wstring text = PlacementCaption(*clip, placement);

                if (clip->Kind == ClipKind::Generator && !context.GeneratedAsItPlays.empty())
                {
                    text += L" \u00B7 ";
                    text += context.GeneratedAsItPlays;
                }

                ds.DrawText(winrt::hstring{ text }, textX, y0, available, CaptionHeight, captionColor, m_caption);
            }

            Rect const body{ x0, y0 + CaptionHeight, w, h - CaptionHeight };
            DrawNotes(ds, *clip, clip->Notes, body, placement.Tick, placement.Tick + length,
                context.View.PixelsPerTick, -context.View.ScrollX, noteColor, clip->Kind == ClipKind::Generator);

            // Where a looping clip starts again: from the first mark in view, and only when the
            // marks are far enough apart to see. Closer than that they'd be a smear, and a file
            // could ask for billions of them.
            if (clip->Loop && length > clip->Length && static_cast<double>(clip->Length) * context.View.PixelsPerTick >= 3.0)
            {
                auto const loopColor = colors.Light ? Rgba(0, 0, 0, 0.35) : Rgba(255, 255, 255, 0.35);
                auto const dashed = DashedStroke(2.0f, 2.0f);
                auto const leftTick = static_cast<int64_t>(std::max(0.0, context.View.ScrollX / context.View.PixelsPerTick));
                auto at = placement.Tick + clip->Length;

                if (leftTick > at)
                {
                    at += ((leftTick - at) / clip->Length) * clip->Length;
                }

                for (; at < placement.Tick + length; at += clip->Length)
                {
                    auto const x = static_cast<float>(context.View.XAtTick(at));

                    if (x > width)
                    {
                        break;
                    }

                    if (x >= 0)
                    {
                        ds.DrawLine(x, y0 + CaptionHeight, x, y0 + h, loopColor, 1.0f, dashed);
                    }
                }
            }

            layer.Close();
        }

        if (selected)
        {
            ds.DrawRoundedRectangle(drawX0, y0, drawX1 - drawX0, h, 4.0f, 4.0f, colors.SelectionStroke, 2.0f);
        }
        else if (clip->Kind == ClipKind::Generator)
        {
            ds.DrawRoundedRectangle(drawX0 + 0.5f, y0 + 0.5f, drawX1 - drawX0 - 1.0f, h - 1.0f, 3.5f, 3.5f, stroke, 1.0f, DashedStroke(3.0f, 2.0f));
        }
        else
        {
            ds.DrawRoundedRectangle(drawX0 + 0.5f, y0 + 0.5f, drawX1 - drawX0 - 1.0f, h - 1.0f, 3.5f, 3.5f, stroke, 1.0f);
        }
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawNotes(
        canvas::CanvasDrawingSession const& ds,
        Clip const& clip,
        std::vector<Note> const& notes,
        Rect const& area,
        int64_t firstPassTick,
        int64_t endTick,
        double pixelsPerTick,
        double areaStartTick,
        Color color,
        bool dashed)
    {
        // areaStartTick is the x of tick 0 in this drawing: x = areaStartTick + tick * pixelsPerTick.
        auto const originX = areaStartTick;

        if (notes.empty() || area.Height < 3.0f || clip.Length <= 0)
        {
            return;
        }

        auto const [low, high] = PitchRange(clip, notes);
        auto const rows = static_cast<double>(high - low + 1);
        auto const usable = static_cast<double>(area.Height) - 2.0;
        auto const rowHeight = std::max(1.2, usable / rows);
        auto const noteHeight = static_cast<float>(std::max(1.4, rowHeight - 0.4));

        // The visible part of the area, in ticks.
        auto const leftX = std::max<double>(area.X, -2.0);
        auto const tickAt = [&](double x) { return static_cast<int64_t>(std::floor((x - originX) / pixelsPerTick)); };
        auto const visibleFrom = tickAt(leftX);
        auto const visibleTo = tickAt(static_cast<double>(area.X) + area.Width) + 1;

        size_t drawn{ 0 };
        size_t passesDrawn{ 0 };

        auto const passes = clip.Loop ? std::max<int64_t>(1, (endTick - firstPassTick + clip.Length - 1) / clip.Length) : 1;
        auto const firstPass = clip.Loop ? std::max<int64_t>(0, (visibleFrom - firstPassTick) / clip.Length - 1) : 0;

        // A very short loop over a long stretch has more passes than pixels: at most as many as
        // the outlines draw, so a frame never runs away.
        for (auto pass = firstPass; pass < passes && passesDrawn < MaximumDrawnPasses; ++pass, ++passesDrawn)
        {
            auto const passStart = firstPassTick + pass * clip.Length;
            auto const passEnd = std::min(endTick, passStart + clip.Length);

            if (passStart >= visibleTo || passStart >= endTick)
            {
                break;
            }

            // Notes that start a while before the view can still reach into it.
            auto const searchFrom = std::max<int64_t>(0, visibleFrom - passStart - TicksPerQuarterNote * 8);

            auto first = std::lower_bound(notes.begin(), notes.end(), searchFrom,
                [](Note const& note, int64_t tick) { return note.Tick < tick; });

            for (auto it = first; it != notes.end(); ++it)
            {
                auto const start = passStart + it->Tick;

                if (start >= passEnd || start >= visibleTo)
                {
                    break;
                }

                auto const end = std::min(start + std::max<int64_t>(1, it->Length), passEnd);

                if (end < visibleFrom)
                {
                    continue;
                }

                auto const x = static_cast<float>(originX + static_cast<double>(start) * pixelsPerTick);
                auto const noteWidth = static_cast<float>(std::max(1.4, static_cast<double>(end - start) * pixelsPerTick - 0.5));
                auto const y = area.Y + 1.0f + static_cast<float>((static_cast<double>(high) - it->Number) / rows * usable);
                auto const opacity = 0.55 + 0.45 * (static_cast<double>(it->Velocity) / 65535.0);

                if (dashed)
                {
                    ds.FillRoundedRectangle(x, y, noteWidth, noteHeight, 0.6f, 0.6f, WithAlpha(color, 0.25));
                }
                else
                {
                    ds.FillRoundedRectangle(x, y, noteWidth, noteHeight, 0.6f, 0.6f, WithAlpha(color, opacity));
                }

                if (++drawn >= MaximumPreviewNotes)
                {
                    return;
                }
            }
        }
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawTag(
        canvas::CanvasDrawingSession const& ds,
        Tag const& tag,
        float x,
        float top,
        float height,
        bool atTop,
        ArrangeDrawContext const& context)
    {
        if (x < -200.0f || tag.Text.empty())
        {
            return;
        }

        auto rgb = tag.Color.value_or(0xD7D7D7);
        auto color = FromRgb(rgb);
        auto textColor = FromRgb(TagTextColor(rgb));

        // A theme can make the label a mix of the tag's color with its own paper.
        if (context.Colors != nullptr && context.Colors->Theme != nullptr && context.Colors->Theme->HasPart(L"timeline.tag"))
        {
            auto const label = context.Colors->Theme->Part(L"timeline.tag", {}, ToThemeColor(color));

            if (!label->Background.empty())
            {
                color = ToUiColor(label->Background.front().Solid);
                rgb = (static_cast<uint32_t>(color.R) << 16) | (static_cast<uint32_t>(color.G) << 8) | color.B;
            }

            textColor = label->Foreground.has_value() ? ToUiColor(*label->Foreground) : FromRgb(TagTextColor(rgb));
        }

        ds.DrawLine(x + 0.5f, top, x + 0.5f, top + height - 1.0f, WithAlpha(color, 0.85), 1.0f, DashedStroke(3.0f, 3.0f));

        winrt::hstring const text{ tag.Text };
        auto const textWidth = std::min(220.0f, MeasureWidth(ds, text, m_tagText));
        auto const labelWidth = 12.0f + textWidth + 6.0f;
        auto const labelHeight = 16.0f;
        auto const y = atTop ? top + 3.0f : top + height - 5.0f - labelHeight;

        // A label whose point sits on the moment (seq.css .tag).
        canvasGeometry::CanvasPathBuilder path{ ds };
        path.BeginFigure(x, y + labelHeight / 2.0f);
        path.AddLine(x + 7.0f, y);
        path.AddLine(x + labelWidth - 3.0f, y);
        path.AddQuadraticBezier(foundation::Numerics::float2{ x + labelWidth, y }, foundation::Numerics::float2{ x + labelWidth, y + 3.0f });
        path.AddLine(x + labelWidth, y + labelHeight - 3.0f);
        path.AddQuadraticBezier(foundation::Numerics::float2{ x + labelWidth, y + labelHeight }, foundation::Numerics::float2{ x + labelWidth - 3.0f, y + labelHeight });
        path.AddLine(x + 7.0f, y + labelHeight);
        path.EndFigure(canvasGeometry::CanvasFigureLoop::Closed);

        ds.FillGeometry(canvasGeometry::CanvasGeometry::CreatePath(path), color);
        ds.FillCircle(x + 7.5f, y + 8.0f, 1.5f, Rgba(0, 0, 0, 0.45));
        ds.DrawText(text, x + 12.0f, y, textWidth + 2.0f, labelHeight, textColor, m_tagText);
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawLauncher(
        canvas::CanvasDrawingSession const& ds,
        float width,
        float height,
        std::vector<ArrangeRow> const& rows,
        double scrollY,
        bool fillBelow,
        ArrangeDrawContext const& context)
    {
        if (context.Doc == nullptr || context.Colors == nullptr)
        {
            return;
        }

        EnsureFormats();

        auto const& colors = *context.Colors;
        auto const& doc = *context.Doc;
        auto const sceneCount = doc.Scenes.size();

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);

        float bottom{ 0 };

        for (auto const& row : rows)
        {
            auto const top = static_cast<float>(row.Top - scrollY);
            auto const rowHeight = static_cast<float>(row.Height);
            bottom = top + rowHeight;

            if (top > height)
            {
                break;
            }

            if (bottom < 0)
            {
                continue;
            }

            ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
            ds.FillRectangle(0, top, width, rowHeight, colors.LauncherBackground);
            ds.FillRectangle(0, bottom - 1.0f, width, 1.0f, colors.Divider);

            if (row.Kind != ArrangeRowKind::Track)
            {
                continue;
            }

            auto const track = FindTrack(doc, row.TrackId);

            if (track == nullptr)
            {
                continue;
            }

            auto const muted = track->Muted || row.InsideMutedFolder;

            for (size_t scene = 0; scene < sceneCount; ++scene)
            {
                auto const slotX = static_cast<float>(static_cast<double>(scene) * context.SceneWidth - context.SceneScrollX);
                auto const slotWidth = static_cast<float>(context.SceneWidth);

                if (slotX > width || slotX + slotWidth < 0)
                {
                    continue;
                }

                if (scene + 1 < sceneCount)
                {
                    ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
                    ds.FillRectangle(slotX + slotWidth - 1.0f, top, 1.0f, rowHeight - 1.0f, colors.Divider);
                }

                ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

                Rect const slot{ slotX, top, slotWidth, rowHeight };
                auto const drag = context.ClipDrag.has_value() ? &*context.ClipDrag : nullptr;

                if (drag != nullptr && !drag->Copy && drag->FromSlot == scene && drag->FromTrackId == track->Id)
                {
                    auto faded = ds.CreateLayer(0.35f);
                    DrawCell(ds, *track, scene, slot, muted, context);
                    faded.Close();
                }
                else
                {
                    DrawCell(ds, *track, scene, slot, muted, context);
                }

                // Where a dragged clip would land.
                if (drag != nullptr && drag->ToSlot == scene && drag->ToTrackId == track->Id)
                {
                    auto const inset = rowHeight < static_cast<float>(OneLineRowHeight) ? 2.0f : 4.0f;
                    Rect const cell{ slot.X + inset, slot.Y + inset, slot.Width - inset * 2.0f - 1.0f, slot.Height - inset * 2.0f - 1.0f };

                    ds.FillRoundedRectangle(cell, 4.0f, 4.0f, WithAlpha(colors.SelectionStroke, colors.Light ? 0.14 : 0.22));
                    ds.DrawRoundedRectangle(cell.X - 0.5f, cell.Y - 0.5f, cell.Width + 1.0f, cell.Height + 1.0f, 4.5f, 4.5f, colors.SelectionStroke, 2.0f, DashedStroke(4.0f, 2.0f));

                    if (auto const clip = FindClip(doc, drag->ClipId); clip != nullptr)
                    {
                        ds.DrawText(winrt::hstring{ clip->Name }, cell.X + 6.0f, cell.Y, std::max(1.0f, cell.Width - 10.0f), std::min(cell.Height, 20.0f), colors.Text, m_cellName);
                    }
                }
            }
        }

        if (fillBelow && bottom < height)
        {
            ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
            ds.FillRectangle(0, std::max(0.0f, bottom), width, height - std::max(0.0f, bottom), colors.LauncherBackground);
        }

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
        ds.FillRectangle(width - 1.0f, 0, 1.0f, height, colors.LauncherEdge);
        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawCell(
        canvas::CanvasDrawingSession const& ds,
        Track const& track,
        size_t scene,
        Rect const& slot,
        bool muted,
        ArrangeDrawContext const& context)
    {
        if (DrawThemedCell(ds, track, scene, slot, muted, context))
        {
            return;
        }

        auto const& colors = *context.Colors;

        // seq.css .slot: 4 pixels of padding inside a slot whose right pixel is the divider. A
        // compact track's slots have room for the name and nothing else.
        auto const compact = slot.Height < static_cast<float>(OneLineRowHeight);
        auto const inset = compact ? 2.0f : 4.0f;
        Rect const cell{ slot.X + inset, slot.Y + inset, slot.Width - inset * 2.0f - 1.0f, slot.Height - inset * 2.0f - 1.0f };
        auto const textTop = compact ? cell.Y + (cell.Height - 13.0f) / 2.0f : cell.Y + 3.0f;

        auto const clipId = scene < track.Slots.size() ? track.Slots[scene] : std::wstring{};
        auto const armed = context.Armed != nullptr && context.Armed->contains(track.Id);
        auto const recordingHere = context.RecordingSlot.has_value() && context.RecordingSlot->TrackId == track.Id && context.RecordingSlot->Scene == scene;

        TrackLaunchView const* launch{ nullptr };

        if (context.Launch != nullptr)
        {
            if (auto found = context.Launch->find(track.Id); found != context.Launch->end())
            {
                launch = &found->second;
            }
        }

        if (recordingHere)
        {
            ds.FillRoundedRectangle(cell, 4.0f, 4.0f, WithAlpha(colors.Record, colors.Light ? 0.16 : 0.30));
            ds.DrawRoundedRectangle(cell.X + 0.5f, cell.Y + 0.5f, cell.Width - 1.0f, cell.Height - 1.0f, 3.5f, 3.5f, colors.Record, 1.0f);
            ds.DrawText(L"\uE7C8", cell.X + 4.0f, cell.Y + 3.0f, 10.0f, 13.0f, colors.Record, m_cellGlyph);
            ds.DrawText(context.RecordingCaption, cell.X + 16.0f, cell.Y + 3.0f, cell.Width - 20.0f, 13.0f, colors.Text, m_cellName);

            auto const progress = static_cast<float>(std::clamp(context.RecordingSlot->Progress, 0.0, 1.0));
            ds.FillRectangle(cell.X, cell.Y + cell.Height - 3.0f, cell.Width * progress, 3.0f, colors.Record);

            if (!context.RecordingSlot->Caption.empty())
            {
                winrt::hstring const when{ context.RecordingSlot->Caption };
                auto const w = MeasureWidth(ds, when, m_badge) + 8.0f;
                ds.FillRoundedRectangle(cell.X + 4.0f, cell.Y + cell.Height - 17.0f, w, 13.0f, 3.0f, 3.0f, Rgba(0, 0, 0, 0.55));
                ds.DrawText(when, cell.X + 8.0f, cell.Y + cell.Height - 17.0f, w, 13.0f, Rgba(255, 255, 255), m_badge);
            }

            return;
        }

        auto const clip = clipId.empty() ? nullptr : FindClip(*context.Doc, clipId);

        if (clip == nullptr)
        {
            ds.FillRoundedRectangle(cell, 4.0f, 4.0f, colors.EmptySlotFill);
            ds.DrawRoundedRectangle(cell.X + 0.5f, cell.Y + 0.5f, cell.Width - 1.0f, cell.Height - 1.0f, 3.5f, 3.5f, colors.EmptySlotStroke, 1.0f);
            ds.DrawText(armed ? L"\uE7C8" : L"\uE71A", cell, armed ? colors.ArmedSlotGlyph : colors.EmptySlotGlyph, m_slotGlyph);

            if (context.SelectedSlotTrackId == track.Id && context.SelectedSlot == scene)
            {
                ds.DrawRoundedRectangle(cell.X - 1.5f, cell.Y - 1.5f, cell.Width + 3.0f, cell.Height + 3.0f, 5.0f, 5.0f, colors.SelectionStroke, 1.5f);
            }

            return;
        }

        auto const playing = launch != nullptr && launch->Plays(clipId, scene);
        auto const queued = launch != nullptr && launch->Queues(clipId, scene);

        auto const tc = ClipColor(colors, *clip, track);
        auto const fill = Muted(colors, CellFill(colors, tc, playing), muted);
        auto const stroke = Muted(colors, CellStroke(colors, tc), muted);

        ds.FillRoundedRectangle(cell, 4.0f, 4.0f, fill);

        auto glyphColor = colors.Text2;

        if (playing)
        {
            ds.DrawRoundedRectangle(cell.X - 0.5f, cell.Y - 0.5f, cell.Width + 1.0f, cell.Height + 1.0f, 4.5f, 4.5f, WithAlpha(colors.Play, 0.45), 1.0f);
            ds.DrawRoundedRectangle(cell.X + 0.5f, cell.Y + 0.5f, cell.Width - 1.0f, cell.Height - 1.0f, 3.5f, 3.5f, colors.Play, 1.0f);
            glyphColor = colors.Play;
        }
        else if (queued)
        {
            auto const ring = colors.Light ? Rgba(0, 0, 0, 0.7) : Rgba(255, 255, 255, 0.85);
            ds.DrawRoundedRectangle(cell.X + 0.5f, cell.Y + 0.5f, cell.Width - 1.0f, cell.Height - 1.0f, 3.5f, 3.5f, ring, 1.0f, DashedStroke(3.0f, 2.0f));
            glyphColor = colors.Light ? FromRgb(0x000000) : FromRgb(0xFFFFFF);
        }
        else
        {
            ds.DrawRoundedRectangle(cell.X + 0.5f, cell.Y + 0.5f, cell.Width - 1.0f, cell.Height - 1.0f, 3.5f, 3.5f, stroke, 1.0f);
        }

        if (context.SelectedSlotTrackId == track.Id && context.SelectedSlot == scene)
        {
            ds.DrawRoundedRectangle(cell.X - 1.5f, cell.Y - 1.5f, cell.Width + 3.0f, cell.Height + 3.0f, 5.0f, 5.0f, colors.SelectionStroke, 1.5f);
        }

        ds.DrawText(L"\uE768", cell.X + 4.0f, textTop, 10.0f, 13.0f, glyphColor, m_cellGlyph);
        ds.DrawText(winrt::hstring{ clip->Name }, cell.X + 16.0f, textTop, std::max(1.0f, cell.Width - 20.0f), 13.0f, colors.Text, m_cellName);

        if (!compact)
        {
            auto const kindColor = colors.Light ? Rgba(0, 0, 0, 0.6) : Rgba(255, 255, 255, 0.8);
            DrawKindMark(ds, IconFor(*clip), cell.X + cell.Width - 13.0f, cell.Y + cell.Height - 13.0f, 10.0f, kindColor);

            Rect const mini{ cell.X + 4.0f, cell.Y + 17.0f, std::max(1.0f, cell.Width - 18.0f), std::max(1.0f, cell.Height - 21.0f) };
            auto layer = ds.CreateLayer(1.0f, mini);
            auto const pixelsPerTick = static_cast<double>(mini.Width) / static_cast<double>(clip->Length);

            DrawNotes(ds, *clip, clip->Notes, mini, 0, clip->Length, pixelsPerTick, mini.X,
                Muted(colors, tc, muted), clip->Kind == ClipKind::Generator);

            layer.Close();
        }

        if (playing)
        {
            auto const progress = static_cast<float>(std::clamp(launch->Progress, 0.0, 1.0));
            ds.FillRectangle(cell.X, cell.Y + cell.Height - 3.0f, cell.Width * progress, 3.0f, colors.Play);
        }

        if (queued && !context.BarFormat.empty() && context.Doc != nullptr)
        {
            auto const bar = BarPositionAtTick(context.Doc->Meter, launch->PendingTick).Bar;
            winrt::hstring when{ std::vformat(std::wstring_view{ context.BarFormat }, std::make_wformat_args(bar)) };

            auto const w = MeasureWidth(ds, when, m_badge) + 8.0f;
            ds.FillRoundedRectangle(cell.X + 4.0f, cell.Y + cell.Height - 17.0f, w, 13.0f, 3.0f, 3.0f, Rgba(0, 0, 0, 0.55));
            ds.DrawText(when, cell.X + 8.0f, cell.Y + cell.Height - 17.0f, w, 13.0f, Rgba(255, 255, 255), m_badge);
        }
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawKindMark(canvas::CanvasDrawingSession const& ds, ClipIcon icon, float x, float y, float size, Color color)
    {
        // A character from the icon font, when there's one for this kind.
        if (auto const glyph = ClipIconGlyph(icon); glyph != nullptr)
        {
            ds.DrawText(glyph, Rect{ x - 1.0f, y - 1.0f, size + 2.0f, size + 2.0f }, color, m_kindGlyph);
            return;
        }

        // The comps' symbols are drawn on a 16 unit square, the same as ClipIconPath.
        auto const s = size / 16.0f;

        // Section 28's are drawn from the same parts as their PathIcons.
        if (DrawClipIcon(ds, icon, x, y, size, color))
        {
            return;
        }

        switch (icon)
        {
        case ClipIcon::Euclidean:
            ds.DrawCircle(x + 8.0f * s, y + 8.0f * s, 5.6f * s, color, 1.2f * s);
            ds.FillCircle(x + 8.0f * s, y + 2.4f * s, 2.0f * s, color);
            ds.FillCircle(x + 13.3f * s, y + 9.7f * s, 2.0f * s, color);
            ds.FillCircle(x + 4.0f * s, y + 12.0f * s, 2.0f * s, color);
            break;

        case ClipIcon::Notes:
        default:
            ds.FillRoundedRectangle(x + 1.0f * s, y + 3.0f * s, 6.0f * s, 2.4f * s, s, s, color);
            ds.FillRoundedRectangle(x + 6.0f * s, y + 7.0f * s, 8.0f * s, 2.4f * s, s, s, color);
            ds.FillRoundedRectangle(x + 3.0f * s, y + 11.0f * s, 5.0f * s, 2.4f * s, s, s, color);
            break;
        }
    }
}
