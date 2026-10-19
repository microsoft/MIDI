// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ArrangeRenderer.h"

// The timeline and launcher parts a theme draws as more than a color (Themes\*.json "parts"): the
// field under the lanes, clips, launcher pads and empty slots, and the loop. Each draws only when
// the theme has the part; otherwise the comps' own drawing in ArrangeRenderer.cpp runs.

using winrt::Windows::Foundation::Rect;

namespace midisequencer
{
    namespace
    {
        constexpr wchar_t TextFont[] = L"Segoe UI Variable Text";
        constexpr wchar_t GlyphFont[] = L"Segoe Fluent Icons";
        constexpr float CaptionHeight = 14.0f;

        themeengine::PaintRect ToPaint(Rect const& rect) noexcept
        {
            return { rect.X, rect.Y, rect.Width, rect.Height };
        }

        Color Filtered(themeengine::Color const& color, themeengine::FilterValue const& filter) noexcept
        {
            return themeengine::FilteredColor(color, filter);
        }

        Color PartColor(themeengine::ResolvedStyle const& style, std::wstring_view name, Color fallback) noexcept
        {
            return Filtered(style.ColorOr(name, ToThemeColor(fallback)), style.Filter);
        }

        float MeasureText(canvas::CanvasDrawingSession const& ds, winrt::hstring const& text, canvasText::CanvasTextFormat const& format)
        {
            canvasText::CanvasTextLayout layout{ ds, text, format, 1000.0f, 100.0f };
            return static_cast<float>(layout.LayoutBounds().Width);
        }

        // What a theme can do needs a theme with the part, and the canvas to draw it with.
        std::shared_ptr<themeengine::ResolvedTheme const> ThemeWith(ArrangeDrawContext const& context, std::wstring_view part)
        {
            if (context.Colors == nullptr || context.Colors->Theme == nullptr || context.Creator == nullptr || !context.Colors->Theme->HasPart(part))
            {
                return nullptr;
            }

            return context.Colors->Theme;
        }
    }

    _Use_decl_annotations_
    canvasText::CanvasTextFormat ArrangeRenderer::ThemedFormat(std::wstring const& family, float size, uint16_t weight, bool centered)
    {
        auto const key = std::format(L"{}|{:.2f}|{}|{}", family, size, weight, centered);

        if (auto const found = m_themedFormats.find(key); found != m_themedFormats.end())
        {
            return found->second;
        }

        canvasText::CanvasTextFormat format{};
        format.FontFamily(family);
        format.FontSize(size);
        format.FontWeight(winrt::Windows::UI::Text::FontWeight{ weight });
        format.WordWrapping(canvasText::CanvasWordWrapping::NoWrap);
        format.TrimmingGranularity(canvasText::CanvasTextTrimmingGranularity::Character);
        format.TrimmingSign(canvasText::CanvasTrimmingSign::Ellipsis);
        format.VerticalAlignment(canvasText::CanvasVerticalAlignment::Center);
        format.HorizontalAlignment(centered ? canvasText::CanvasHorizontalAlignment::Center : canvasText::CanvasHorizontalAlignment::Left);

        m_themedFormats.emplace(key, format);
        return format;
    }

    _Use_decl_annotations_
    canvas::CanvasActiveLayer ArrangeRenderer::DrawField(canvas::CanvasDrawingSession const& ds, float width, float height, bool pinnedCanvas, ArrangeDrawContext const& context)
    {
        auto const theme = ThemeWith(context, L"arrange.field");

        if (theme == nullptr)
        {
            return nullptr;
        }

        auto const field = theme->Part(L"arrange.field");
        auto const& margin = field->Margin;
        auto const radius = static_cast<float>(field->Radius[0].Value);

        // The rows that stay put and the ones that scroll are two canvases with the pin edge
        // between them: the field's top corners are on the first and its bottom ones on the
        // second, and each runs past the edge where it meets the other.
        auto const left = static_cast<float>(margin[3]);
        auto const right = static_cast<float>(margin[1]);

        themeengine::PaintRect const rect = pinnedCanvas
            ? themeengine::PaintRect{ left, static_cast<float>(margin[0]), width - left - right, height - static_cast<float>(margin[0]) + radius * 2.0f }
            : themeengine::PaintRect{ left, -radius * 2.0f, width - left - right, height + radius * 2.0f - static_cast<float>(margin[2]) };

        if (rect.Width <= 0 || rect.Height <= 0)
        {
            return nullptr;
        }

        m_painter.Paint(ds, context.Creator, *field, rect);
        return ds.CreateLayer(1.0f, themeengine::PartShape(context.Creator, *field, rect));
    }

    _Use_decl_annotations_
    void ArrangeRenderer::DrawRowSeam(canvas::CanvasDrawingSession const& ds, float bottom, float width, Palette const& colors)
    {
        ds.FillRectangle(0, bottom - 1.0f, width, 1.0f, colors.Divider);

        // An engraved seam: a dark line along the bottom with a light one above it.
        if (colors.LaneSeamLight.A > 0)
        {
            ds.FillRectangle(0, bottom - 2.0f, width, 1.0f, colors.LaneSeamLight);
        }

        if (colors.LaneSeam.A > 0)
        {
            ds.FillRectangle(0, bottom - 1.0f, width, 1.0f, colors.LaneSeam);
        }
    }

    _Use_decl_annotations_
    bool ArrangeRenderer::DrawThemedLoop(canvas::CanvasDrawingSession const& ds, float x0, float x1, float height, ArrangeDrawContext const& context)
    {
        auto const theme = ThemeWith(context, L"ruler.loop");

        if (theme == nullptr)
        {
            return false;
        }

        auto const loop = theme->Part(L"ruler.loop");
        auto const bandHeight = static_cast<float>(loop->Height.value_or(8.0));

        ds.Antialiasing(canvas::CanvasAntialiasing::Antialiased);

        // A loop that's off is the same band, fainter.
        auto layer = ds.CreateLayer(context.LoopEnabled ? 1.0f : 0.4f);
        m_painter.Paint(ds, context.Creator, *loop, { x0, height - bandHeight, x1 - x0, bandHeight });
        layer.Close();

        ds.Antialiasing(canvas::CanvasAntialiasing::Aliased);
        return true;
    }

    _Use_decl_annotations_
    bool ArrangeRenderer::DrawThemedClip(
        canvas::CanvasDrawingSession const& ds,
        Track const& track,
        Clip const& clip,
        Placement const& placement,
        bool selected,
        bool muted,
        float top,
        float height,
        float width,
        ArrangeDrawContext const& context)
    {
        auto const theme = ThemeWith(context, L"timeline.clip");

        if (theme == nullptr)
        {
            return false;
        }

        auto const& colors = *context.Colors;
        auto const length = placement.Length > 0 ? placement.Length : clip.Length;

        auto const x0 = static_cast<float>(context.View.XAtTick(placement.Tick)) + 1.0f;
        auto const x1 = std::max(x0 + 2.0f, static_cast<float>(context.View.XAtTick(placement.Tick + length)) - 1.0f);

        if (x1 < -4.0f || x0 > width + 4.0f)
        {
            return true;
        }

        std::vector<std::wstring_view> states{};

        if (clip.Kind == ClipKind::Generator) { states.push_back(L"generator"); }
        if (selected) { states.push_back(L"selected"); }
        if (muted) { states.push_back(L"muted"); }

        auto const trackColor = ThemedTrackColor(colors, FromRgb(clip.Color.value_or(track.Color) & 0xFFFFFF));
        auto const style = theme->Part(L"timeline.clip", std::span<std::wstring_view const>{ states }, ToThemeColor(trackColor));

        auto const inset = height < static_cast<float>(OneLineRowHeight) ? 2.0f : static_cast<float>(style->Number(L"inset", 4.0));
        auto const y0 = top + inset;
        auto const h = height - inset * 2.0f;

        // A block hundreds of thousands of pixels wide is drawn only as far as the view.
        auto const drawX0 = std::max(x0, -24.0f);
        auto const drawX1 = std::min(x1, width + 24.0f);
        themeengine::PaintRect const rect{ drawX0, y0, drawX1 - drawX0, h };

        // The whole clip fades together, as CSS fades an element with opacity.
        canvas::CanvasActiveLayer fade{ nullptr };

        if (style->Opacity < 1.0)
        {
            fade = ds.CreateLayer(static_cast<float>(style->Opacity));
        }

        m_painter.Paint(ds, context.Creator, *style, rect, false);

        {
            auto inside = ds.CreateLayer(1.0f, themeengine::PartShape(context.Creator, *style, rect));

            auto const captionHeight = static_cast<float>(style->Number(L"caption-height", CaptionHeight));
            auto textX = std::max(x0, 0.0f) + static_cast<float>(style->Number(L"caption-indent", 7.0));

            // The track's color as a dot before the name, or a ring for a clip made as it plays.
            if (auto const dot = static_cast<float>(style->Number(L"dot-size", 0.0)); dot > 0)
            {
                auto const dotColor = PartColor(*style, L"dot-color", trackColor);
                auto const cx = textX + dot / 2.0f;
                auto const cy = y0 + captionHeight / 2.0f;

                if (style->Word(L"dot-style", L"fill") == L"ring")
                {
                    ds.DrawCircle(cx, cy, dot / 2.0f - 1.0f, dotColor, 2.0f);
                }
                else
                {
                    ds.FillCircle(cx, cy, dot / 2.0f, dotColor);
                }

                textX += dot + 4.0f;
            }

            if (UseCount(clip.Id) > 1)
            {
                ds.DrawText(L"\uE71B", textX, y0, 10.0f, captionHeight, PartColor(*style, L"link-color", colors.Text3), ThemedFormat(GlyphFont, 8.0f, 400));
                textX += 12.0f;
            }

            if (auto const available = x1 - textX - 4.0f; available > 4.0f)
            {
                std::wstring text = PlacementCaption(clip, placement);

                if (clip.Kind == ClipKind::Generator && !context.GeneratedAsItPlays.empty())
                {
                    text += L" \u00B7 ";
                    text += context.GeneratedAsItPlays;
                }

                auto const family = theme->Font(style->Word(L"caption-font", {}), TextFont);
                auto const format = ThemedFormat(family, static_cast<float>(style->Number(L"caption-size", 10.0)), static_cast<uint16_t>(style->Number(L"caption-weight", 400.0)));
                ds.DrawText(winrt::hstring{ text }, textX, y0, available, captionHeight, PartColor(*style, L"caption-color", colors.Text), format);
            }

            Rect const body{ x0, y0 + captionHeight, x1 - x0, h - captionHeight };
            DrawNotes(ds, clip, clip.Notes, body, placement.Tick, placement.Tick + length, context.View.PixelsPerTick, -context.View.ScrollX,
                PartColor(*style, L"note-color", trackColor), clip.Kind == ClipKind::Generator);

            // Where a looping clip starts again, as the comps' own drawing marks it.
            if (clip.Loop && length > clip.Length && static_cast<double>(clip.Length) * context.View.PixelsPerTick >= 3.0)
            {
                canvasGeometry::CanvasStrokeStyle dashed{};
                dashed.CustomDashStyle({ 2.0f, 2.0f });

                auto const loopColor = PartColor(*style, L"loop-color", WithAlpha(colors.Text3, 0.4));
                auto const leftTick = static_cast<int64_t>(std::max(0.0, context.View.ScrollX / context.View.PixelsPerTick));
                auto at = placement.Tick + clip.Length;

                if (leftTick > at)
                {
                    at += ((leftTick - at) / clip.Length) * clip.Length;
                }

                for (; at < placement.Tick + length; at += clip.Length)
                {
                    auto const x = static_cast<float>(context.View.XAtTick(at));

                    if (x > width)
                    {
                        break;
                    }

                    if (x >= 0)
                    {
                        ds.DrawLine(x, y0 + captionHeight, x, y0 + h, loopColor, 1.0f, dashed);
                    }
                }
            }

            inside.Close();
        }

        if (fade != nullptr)
        {
            fade.Close();
        }

        return true;
    }

    _Use_decl_annotations_
    bool ArrangeRenderer::DrawThemedCell(
        canvas::CanvasDrawingSession const& ds,
        Track const& track,
        size_t scene,
        Rect const& slot,
        bool muted,
        ArrangeDrawContext const& context)
    {
        auto const theme = ThemeWith(context, L"launcher.cell");

        if (theme == nullptr)
        {
            return false;
        }

        auto const& colors = *context.Colors;
        auto const compact = slot.Height < static_cast<float>(OneLineRowHeight);
        auto const selected = context.SelectedSlotTrackId == track.Id && context.SelectedSlot == scene;

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

        auto const clip = clipId.empty() ? nullptr : FindClip(*context.Doc, clipId);

        // A compact row's pads sit closer to its edges.
        auto const placeIn = [&](themeengine::ResolvedStyle const& style)
        {
            auto space = ToPaint(slot);

            if (compact)
            {
                return themeengine::PaintRect{ space.X + 2.0f, space.Y + 2.0f, std::max(0.0f, space.Width - 4.0f), std::max(0.0f, space.Height - 4.0f) };
            }

            return themeengine::PartRect(style, space);
        };

        auto const drawBadge = [&](winrt::hstring const& text, themeengine::PaintRect const& cell)
        {
            auto const badge = theme->Part(L"launcher.when");
            auto const format = ThemedFormat(TextFont, 9.5f, 400);
            auto const w = MeasureText(ds, text, format) + 8.0f;
            themeengine::PaintRect const rect{ cell.X + 4.0f, cell.Y + cell.Height - 17.0f, w, 13.0f };

            m_painter.Paint(ds, context.Creator, *badge, rect);
            ds.DrawText(text, rect.X + 4.0f, rect.Y, w, rect.Height, PartColor(*badge, L"color", badge->Foreground.has_value() ? ToUiColor(*badge->Foreground) : colors.Text), format);
        };

        auto const drawProgress = [&](themeengine::PaintRect const& cell, double progress, bool recording)
        {
            std::wstring_view const states[]{ L"recording" };
            auto const bar = theme->Part(L"launcher.progress", recording ? std::span<std::wstring_view const>{ states } : std::span<std::wstring_view const>{});
            auto const barHeight = static_cast<float>(bar->Height.value_or(3.0));
            auto const left = static_cast<float>(bar->Margin[3]);
            auto const right = static_cast<float>(bar->Margin[1]);
            auto const full = cell.Width - left - right;
            auto const amount = static_cast<float>(std::clamp(progress, 0.0, 1.0));

            if (full > 0 && amount > 0)
            {
                m_painter.Paint(ds, context.Creator, *bar, { cell.X + left, cell.Y + cell.Height - static_cast<float>(bar->Margin[2]) - barHeight, full * amount, barHeight });
            }
        };

        auto const nameFormat = [&](themeengine::ResolvedStyle const& style)
        {
            return ThemedFormat(theme->Font(style.Word(L"name-font", {}), TextFont), static_cast<float>(style.Number(L"name-size", 10.5)), 400);
        };

        if (recordingHere)
        {
            std::vector<std::wstring_view> states{ L"recording" };
            if (selected) { states.push_back(L"selected"); }

            auto const style = theme->Part(L"launcher.cell", std::span<std::wstring_view const>{ states });
            auto const cell = placeIn(*style);
            auto const textTop = compact ? cell.Y + (cell.Height - 13.0f) / 2.0f : cell.Y + 3.0f;

            m_painter.Paint(ds, context.Creator, *style, cell);
            ds.DrawText(L"\uE7C8", cell.X + 6.0f, textTop, 10.0f, 13.0f, PartColor(*style, L"glyph-color", colors.Record), ThemedFormat(GlyphFont, 8.0f, 400));
            ds.DrawText(context.RecordingCaption, cell.X + 18.0f, textTop, std::max(1.0f, cell.Width - 22.0f), 13.0f, PartColor(*style, L"name-color", colors.Text), nameFormat(*style));
            drawProgress(cell, context.RecordingSlot->Progress, true);

            if (!context.RecordingSlot->Caption.empty())
            {
                drawBadge(winrt::hstring{ context.RecordingSlot->Caption }, cell);
            }

            return true;
        }

        if (clip == nullptr)
        {
            std::vector<std::wstring_view> states{};
            if (armed) { states.push_back(L"armed"); }
            if (selected) { states.push_back(L"selected"); }

            auto const style = theme->Part(L"launcher.empty", std::span<std::wstring_view const>{ states });
            auto const cell = placeIn(*style);

            m_painter.Paint(ds, context.Creator, *style, cell);
            ds.DrawText(armed ? L"\uE7C8" : L"\uE71A", Rect{ cell.X, cell.Y, cell.Width, cell.Height },
                PartColor(*style, L"glyph-color", armed ? colors.ArmedSlotGlyph : colors.EmptySlotGlyph), ThemedFormat(GlyphFont, 8.0f, 400, true));
            return true;
        }

        auto const playing = launch != nullptr && launch->Plays(clipId, scene);
        auto const queued = launch != nullptr && launch->Queues(clipId, scene);

        std::vector<std::wstring_view> states{};
        if (playing) { states.push_back(L"playing"); }
        if (queued) { states.push_back(L"queued"); }
        if (muted) { states.push_back(L"muted"); }
        if (selected) { states.push_back(L"selected"); }

        auto const trackColor = ThemedTrackColor(colors, FromRgb(clip->Color.value_or(track.Color) & 0xFFFFFF));
        auto const style = theme->Part(L"launcher.cell", std::span<std::wstring_view const>{ states }, ToThemeColor(trackColor));
        auto const cell = placeIn(*style);
        auto const textTop = compact ? cell.Y + (cell.Height - 13.0f) / 2.0f : cell.Y + 3.0f;

        canvas::CanvasActiveLayer fade{ nullptr };

        if (style->Opacity < 1.0)
        {
            fade = ds.CreateLayer(static_cast<float>(style->Opacity));
        }

        m_painter.Paint(ds, context.Creator, *style, cell, false);

        ds.DrawText(L"\uE768", cell.X + 6.0f, textTop, 10.0f, 13.0f, PartColor(*style, L"glyph-color", colors.Text2), ThemedFormat(GlyphFont, 8.0f, 400));
        ds.DrawText(winrt::hstring{ clip->Name }, cell.X + 18.0f, textTop, std::max(1.0f, cell.Width - 22.0f), 13.0f, PartColor(*style, L"name-color", colors.Text), nameFormat(*style));

        if (!compact)
        {
            DrawKindMark(ds, IconFor(*clip), cell.X + cell.Width - 14.0f, cell.Y + cell.Height - 14.0f, 10.0f, PartColor(*style, L"kind-color", colors.Text3));

            Rect const mini{ cell.X + 6.0f, cell.Y + 17.0f, std::max(1.0f, cell.Width - 22.0f), std::max(1.0f, cell.Height - 21.0f) };
            auto layer = ds.CreateLayer(1.0f, mini);
            auto const pixelsPerTick = static_cast<double>(mini.Width) / static_cast<double>(std::max<int64_t>(1, clip->Length));

            DrawNotes(ds, *clip, clip->Notes, mini, 0, clip->Length, pixelsPerTick, mini.X, PartColor(*style, L"note-color", trackColor), clip->Kind == ClipKind::Generator);
            layer.Close();
        }

        if (playing)
        {
            drawProgress(cell, launch->Progress, false);
        }

        if (queued && !context.BarFormat.empty() && context.Doc != nullptr)
        {
            auto const bar = BarPositionAtTick(context.Doc->Meter, launch->PendingTick).Bar;
            drawBadge(winrt::hstring{ std::vformat(std::wstring_view{ context.BarFormat }, std::make_wformat_args(bar)) }, cell);
        }

        if (fade != nullptr)
        {
            fade.Close();
        }

        return true;
    }
}
