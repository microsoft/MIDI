// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The timeline's scroll bar and zoom (design section 31): the bar (TimelineBar.h) under the
// timeline or above the ruler, what it shows, the views it asks for, a pinch on the timeline, and
// its two settings.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceRender.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr double TicksPerFourFourBar = 3840.0;

        // The rows ArrangeGrid keeps for the bar: above the ruler, and under the timeline.
        constexpr int32_t TopRow = 0;
        constexpr int32_t BottomRow = 5;

        int64_t PlacementLength(seq::Sequence const& doc, seq::Placement const& placement) noexcept
        {
            if (placement.Length > 0)
            {
                return placement.Length;
            }

            auto const clip = seq::FindClip(doc, placement.ClipId);
            return clip != nullptr ? std::max<int64_t>(1, clip->Length) : seq::TicksPerQuarterNote * 4;
        }

        void AddSpans(seq::Sequence const& doc, seq::Track const& track, std::vector<std::pair<int64_t, int64_t>>& spans)
        {
            for (auto const& placement : track.Timeline)
            {
                spans.emplace_back(placement.Tick, placement.Tick + PlacementLength(doc, placement));
            }

            for (auto const& child : track.Children)
            {
                AddSpans(doc, child, spans);
            }
        }
    }

    void MainWindow::SetUpTimelineBar() noexcept
    {
        try
        {
            auto const weak = get_weak();

            seq::TimelineBarActions actions{};

            actions.Model = [weak]()
            {
                auto strong = weak.get();
                return strong ? strong->TimelineBarState() : seq::TimelineBarModel{};
            };

            actions.SetView = [weak](seq::TimelineSpan const& view)
            {
                if (auto strong = weak.get()) { strong->SetTimelineView(view); }
            };

            actions.ZoomStep = [weak](double factor)
            {
                if (auto strong = weak.get()) { strong->ZoomTimelineStep(factor); }
            };

            actions.ZoomToFit = [weak]()
            {
                if (auto strong = weak.get()) { strong->ZoomTimelineToFit(); }
            };

            actions.ZoomToSelection = [weak]()
            {
                if (auto strong = weak.get()) { strong->ZoomTimelineToSelection(); }
            };

            actions.SetOverview = [weak](bool overview)
            {
                if (auto strong = weak.get()) { strong->SetTimelineOverview(overview); }
            };

            actions.SetAtTop = [weak](bool atTop)
            {
                if (auto strong = weak.get()) { strong->SetTimelineBarAtTop(atTop); }
            };

            auto const host = m_timelineBar.Create(std::move(actions));
            controls::Grid::SetColumnSpan(host, 3);
            ArrangeGrid().Children().Append(host);

            m_timelineBar.Palette(&m_palette);

            // A pinch on a touch screen zooms the timeline about the pinch. A touchpad's comes as
            // Ctrl and the wheel, which OnLaneWheel takes.
            for (auto const& canvas : { m_rulerCanvas, m_pinnedLaneCanvas, m_scrollLaneCanvas })
            {
                canvas.ManipulationMode(input::ManipulationModes::Scale);
                canvas.ManipulationDelta([weak, element = winrt::make_weak(canvas)](auto&&, input::ManipulationDeltaRoutedEventArgs const& args)
                {
                    auto strong = weak.get();
                    auto target = element.get();

                    if (strong && target)
                    {
                        strong->OnTimelinePinch(args, target);
                    }
                });
            }

            PlaceTimelineBar();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to set up the timeline's scroll bar.")
    }

    void MainWindow::PlaceTimelineBar() noexcept
    {
        try
        {
            auto const host = m_timelineBar.Host();

            if (host == nullptr)
            {
                return;
            }

            auto const& settings = seq::AppSettings::Current();
            m_timelineBar.Overview(settings.TimelineOverview());
            m_timelineBar.AtTop(settings.TimelineBarAtTop());

            auto const height = m_timelineBar.RowHeight();
            auto const atTop = settings.TimelineBarAtTop();

            TimelineBarTopRow().Height(xaml::GridLength{ atTop ? height : 0.0, xaml::GridUnitType::Pixel });
            TimelineBarBottomRow().Height(xaml::GridLength{ atTop ? 0.0 : height, xaml::GridUnitType::Pixel });
            controls::Grid::SetRow(host, atTop ? TopRow : BottomRow);

            m_timelineBar.Invalidate();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to place the timeline's scroll bar.")
    }

    _Use_decl_annotations_
    void MainWindow::SetTimelineOverview(bool overview) noexcept
    {
        seq::AppSettings::Current().TimelineOverview(overview);
        PlaceTimelineBar();
    }

    _Use_decl_annotations_
    void MainWindow::SetTimelineBarAtTop(bool atTop) noexcept
    {
        seq::AppSettings::Current().TimelineBarAtTop(atTop);
        PlaceTimelineBar();
    }

    // ---------------------------------------------------------------- the view

    seq::TimelineSpan MainWindow::TimelineView() noexcept
    {
        auto const pixelsPerTick = std::max(1.0e-9, m_barWidth / TicksPerFourFourBar);
        return seq::TimelineSpan{ m_scrollX / pixelsPerTick, LaneWidth() / pixelsPerTick };
    }

    seq::TimelineLimits MainWindow::TimelineLimits() noexcept
    {
        auto const width = LaneWidth();
        auto const view = TimelineView();
        auto const end = std::max<int64_t>(seq::SequenceEndTick(m_doc), m_position);
        auto const bar = static_cast<int64_t>(TicksPerFourFourBar);

        seq::TimelineLimits limits{};
        limits.MinSpan = width * TicksPerFourFourBar / seq::AppSettings::MaximumBarWidth;

        // Out to the sequence and the room after it, or as far out as it is already.
        auto const content = seq::TimelineExtent(end, bar, seq::TimelineSpan{ 0, 0 });
        auto const farthest = width * TicksPerFourFourBar / seq::AppSettings::MinimumBarWidth;
        limits.MaxSpan = std::max(limits.MinSpan, std::min(farthest, std::max(content, view.Span)));
        limits.End = seq::TimelineExtent(end, bar, view);

        return limits;
    }

    _Use_decl_annotations_
    void MainWindow::SetTimelineView(seq::TimelineSpan const& requested) noexcept
    {
        try
        {
            auto const width = LaneWidth();
            auto const view = seq::ClampView(requested, TimelineLimits());

            if (!(view.Span > 0))
            {
                return;
            }

            auto const barWidth = std::clamp(width * TicksPerFourFourBar / view.Span, seq::AppSettings::MinimumBarWidth, seq::AppSettings::MaximumBarWidth);
            auto const zoomed = std::abs(barWidth - m_barWidth) > 1.0e-6;

            m_barWidth = barWidth;
            m_scrollX = std::max(0.0, view.Start * (m_barWidth / TicksPerFourFourBar));

            // Kept in tenths of a pixel, so a drag doesn't write every small change.
            if (zoomed && std::abs(seq::AppSettings::Current().BarWidth() - m_barWidth) >= 0.05)
            {
                seq::AppSettings::Current().BarWidth(m_barWidth);
            }

            UpdateScrollBars();
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to move the timeline.")
    }

    _Use_decl_annotations_
    void MainWindow::ZoomTimelineStep(double factor) noexcept
    {
        try
        {
            m_timelineBar.ForgetFit();

            // About the playhead when it's in view, else the middle.
            auto const view = TimelineView();
            auto const playhead = static_cast<double>(m_position);
            auto const anchor = playhead >= view.Start && playhead <= view.End() ? playhead : view.Start + view.Span / 2.0;

            SetTimelineView(seq::ZoomAbout(view, factor, anchor, TimelineLimits()));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to zoom the timeline.")
    }

    void MainWindow::ZoomTimelineToFit() noexcept
    {
        try
        {
            auto const limits = TimelineLimits();
            auto const end = static_cast<double>(seq::SequenceEndTick(m_doc));

            // The fit can be wider than the zoom-out limit would allow from here.
            auto fit = limits;
            fit.MaxSpan = std::max(limits.MaxSpan, seq::TimelineExtent(static_cast<int64_t>(end), static_cast<int64_t>(TicksPerFourFourBar), seq::TimelineSpan{ 0, 0 }));

            SetTimelineView(seq::FitView(0.0, end, static_cast<int64_t>(TicksPerFourFourBar), fit));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to zoom the timeline to fit.")
    }

    void MainWindow::ZoomTimelineToSelection() noexcept
    {
        try
        {
            m_timelineBar.ForgetFit();

            auto const bar = static_cast<int64_t>(TicksPerFourFourBar);

            if (auto const track = seq::FindTrack(m_doc, m_selectedTrackId); track != nullptr && m_selectedPlacement < track->Timeline.size())
            {
                auto const& placement = track->Timeline[m_selectedPlacement];
                auto const from = static_cast<double>(placement.Tick);
                auto const to = static_cast<double>(placement.Tick + PlacementLength(m_doc, placement));
                SetTimelineView(seq::FitView(from, to, bar, TimelineLimits()));
                return;
            }

            if (m_loopEnd > m_loopStart)
            {
                SetTimelineView(seq::FitView(static_cast<double>(m_loopStart), static_cast<double>(m_loopEnd), bar, TimelineLimits()));
                return;
            }

            ZoomTimelineToFit();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to zoom the timeline to the selection.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTimelinePinch(input::ManipulationDeltaRoutedEventArgs const& args, xaml::UIElement const& canvas)
    {
        try
        {
            auto const scale = static_cast<double>(args.Delta().Scale);

            if (scale > 0.0 && std::abs(scale - 1.0) > 1.0e-4)
            {
                auto position = args.Position();

                if (auto const container = args.Container(); container != nullptr && container != canvas)
                {
                    position = container.TransformToVisual(canvas).TransformPoint(position);
                }

                m_timelineBar.ForgetFit();
                SetZoom(m_barWidth * scale, std::clamp(static_cast<double>(position.X), 0.0, LaneWidth()));
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to zoom the timeline.")
    }

    // ---------------------------------------------------------------- what the bar shows

    seq::TimelineBarModel MainWindow::TimelineBarState()
    {
        seq::TimelineBarModel model{};
        model.View = TimelineView();
        model.Limits = TimelineLimits();
        model.Extent = model.Limits.End;
        model.TicksPerBar = m_doc.Meter.empty() ? static_cast<int64_t>(TicksPerFourFourBar) : seq::TicksPerBar(m_doc.Meter.front());

        auto const host = m_timelineBar.Host();
        auto const lanes = ScrollLaneHost();

        if (host != nullptr && lanes.ActualWidth() > 0)
        {
            model.LaneLeft = lanes.TransformToVisual(host).TransformPoint(foundation::Point{ 0, 0 }).X;
        }

        model.LaneWidth = static_cast<float>(LaneWidth());
        model.HeaderWidth = static_cast<float>(HeaderColumn().ActualWidth());

        model.Playhead = m_position;
        model.LoopStart = m_loopStart;
        model.LoopEnd = m_loopEnd;
        model.LoopOn = LoopToggle().IsChecked() != nullptr && LoopToggle().IsChecked().GetBoolean();

        // A line for each track the timeline shows, in its order: an open folder's tracks have
        // their own, and a closed folder one for all of them.
        auto const addRows = [this, &model](std::vector<seq::ArrangeRow> const& rows)
        {
            for (auto const& row : rows)
            {
                if (row.Kind != seq::ArrangeRowKind::Track && row.Kind != seq::ArrangeRowKind::Folder)
                {
                    continue;
                }

                auto const track = seq::FindTrack(m_doc, row.TrackId);

                if (track == nullptr || (track->IsFolder && track->Open))
                {
                    continue;
                }

                seq::TimelineBarLine line{};
                line.Color = seq::ThemedTrackColor(m_palette, seq::FromRgb(track->Color));
                line.Muted = track->Muted;
                AddSpans(m_doc, *track, line.Spans);
                model.Lines.push_back(std::move(line));
            }
        };

        addRows(m_layout.Pinned);
        addRows(m_layout.Scrolling);

        // The sections: the tempo track's tags.
        for (auto const& tag : m_doc.Tags)
        {
            seq::TimelineBarSection section{};
            section.Tick = tag.Tick;
            section.Name = tag.Text;

            if (tag.Color.has_value())
            {
                section.Color = seq::FromRgb(*tag.Color);
            }

            model.Sections.push_back(std::move(section));
        }

        std::stable_sort(model.Sections.begin(), model.Sections.end(), [](auto const& a, auto const& b) { return a.Tick < b.Tick; });

        // The selected clip, then everywhere else it's placed.
        if (auto const track = seq::FindTrack(m_doc, m_selectedTrackId); track != nullptr && m_selectedPlacement < track->Timeline.size())
        {
            auto const& selected = track->Timeline[m_selectedPlacement];
            model.Linked.emplace_back(selected.Tick, selected.Tick + PlacementLength(m_doc, selected));

            seq::ForEachTrack(m_doc, [&](seq::Track const& other, size_t)
            {
                for (size_t i = 0; i < other.Timeline.size(); ++i)
                {
                    auto const& placement = other.Timeline[i];

                    if (placement.ClipId == selected.ClipId && !(other.Id == track->Id && i == m_selectedPlacement))
                    {
                        model.Linked.emplace_back(placement.Tick, placement.Tick + PlacementLength(m_doc, placement));
                    }
                }

                return true;
            });
        }

        return model;
    }

    // ---------------------------------------------------------------- the settings

    xaml::UIElement MainWindow::MakeTimelineBarSettings()
    {
        controls::StackPanel panel{};
        panel.Orientation(controls::Orientation::Horizontal);
        panel.Spacing(12);

        auto const& settings = seq::AppSettings::Current();

        controls::ComboBox kind{};
        kind.Header(winrt::box_value(res::GetString(L"AppearanceTimelineBar")));
        kind.Width(200.0);
        kind.Items().Append(winrt::box_value(res::GetString(L"TimelineBarOverviewItem")));
        kind.Items().Append(winrt::box_value(res::GetString(L"TimelineBarPlainItem")));
        kind.SelectedIndex(settings.TimelineOverview() ? 0 : 1);
        panel.Children().Append(kind);

        controls::ComboBox place{};
        place.Header(winrt::box_value(res::GetString(L"AppearanceTimelineBarPlace")));
        place.Width(200.0);
        place.Items().Append(winrt::box_value(res::GetString(L"TimelineBarUnderItem")));
        place.Items().Append(winrt::box_value(res::GetString(L"TimelineBarAboveItem")));
        place.SelectedIndex(settings.TimelineBarAtTop() ? 1 : 0);
        panel.Children().Append(place);

        kind.SelectionChanged([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
        {
            if (auto strong = weak.get())
            {
                auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                if (index >= 0)
                {
                    strong->SetTimelineOverview(index == 0);
                }
            }
        });

        place.SelectionChanged([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
        {
            if (auto strong = weak.get())
            {
                auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                if (index >= 0)
                {
                    strong->SetTimelineBarAtTop(index == 1);
                }
            }
        });

        return panel;
    }
}
