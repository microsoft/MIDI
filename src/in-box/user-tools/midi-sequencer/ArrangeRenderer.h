// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Draws the ruler, the timeline lanes and the clip launcher with Win2D, from the sequence and a
// little state the window owns (selection, what's playing, what's recording). Nothing here keeps
// XAML elements, so the cost of a frame follows what's on screen, not the size of the sequence.

#include "ArrangeLayout.h"
#include "PlaybackEngine.h"
#include "SequenceModel.h"
#include "ThemePalette.h"

namespace midisequencer
{
    struct TimelineView
    {
        // A 4/4 bar is BarWidth pixels wide.
        double PixelsPerTick{ 34.0 / 3840.0 };

        // Pixels from tick 0 to the left edge.
        double ScrollX{ 0 };

        double XAtTick(int64_t tick) const noexcept { return static_cast<double>(tick) * PixelsPerTick - ScrollX; }
        int64_t TickAtX(double x) const noexcept { return static_cast<int64_t>(std::floor((x + ScrollX) / PixelsPerTick)); }
    };

    // A launcher slot that's recording, or queued to.
    struct SlotRecording
    {
        std::wstring TrackId{};
        size_t Scene{ 0 };
        double Progress{ 0 };
        std::wstring Caption{};
    };

    struct ArrangeDrawContext
    {
        Sequence const* Doc{ nullptr };
        Palette const* Colors{ nullptr };
        TimelineView View{};

        // The selected placement: its track and its index on that track's timeline.
        std::wstring SelectedTrackId{};
        size_t SelectedPlacement{ SIZE_MAX };

        // The selected launcher slot, for the keyboard.
        std::wstring SelectedSlotTrackId{};
        size_t SelectedSlot{ SIZE_MAX };

        bool LoopEnabled{ false };
        int64_t LoopStart{ 0 };
        int64_t LoopEnd{ 0 };

        // What the launcher is doing, per track id.
        std::unordered_map<std::wstring, TrackLaunchView> const* Launch{ nullptr };

        std::unordered_set<std::wstring> const* Armed{ nullptr };

        // A take being recorded on the timeline: its notes so far, from its start.
        std::wstring RecordingTrackId{};
        int64_t RecordingStartTick{ 0 };
        std::vector<Note> const* RecordingNotes{ nullptr };
        int64_t NowTick{ 0 };

        std::optional<SlotRecording> RecordingSlot{};

        double SceneWidth{ 100 };
        double SceneScrollX{ 0 };

        // Strings that are drawn rather than shown in XAML, from the app's resources.
        winrt::hstring PlayingLaunchedClip{};
        winrt::hstring StoppedFromLauncher{};
        winrt::hstring BackToTimeline{};
        winrt::hstring BarFormat{};             // "Bar {0}"
        winrt::hstring RecordingCaption{};      // "Recording"
        winrt::hstring GeneratedAsItPlays{};
    };

    class ArrangeRenderer
    {
    public:
        void DrawRuler(
            _In_ canvas::CanvasDrawingSession const& ds,
            _In_ float width,
            _In_ float height,
            _In_ ArrangeDrawContext const& context);

        void DrawLanes(
            _In_ canvas::CanvasDrawingSession const& ds,
            _In_ float width,
            _In_ float height,
            _In_ std::vector<ArrangeRow> const& rows,
            _In_ double scrollY,
            _In_ bool fillBelow,
            _In_ ArrangeDrawContext const& context);

        void DrawLauncher(
            _In_ canvas::CanvasDrawingSession const& ds,
            _In_ float width,
            _In_ float height,
            _In_ std::vector<ArrangeRow> const& rows,
            _In_ double scrollY,
            _In_ bool fillBelow,
            _In_ ArrangeDrawContext const& context);

        // Where the "Back to timeline" button is drawn on a track covered by a launched clip, so a
        // click there can be told apart from a click on the lane. Known once it has been drawn.
        foundation::Rect BackToTimelineButtonBounds(_In_ float rowTop, _In_ float rowHeight) const noexcept;

        // The clip's pitch range is worked out once per clip and kept until the sequence changes.
        void InvalidateCaches() noexcept { m_pitchRanges.clear(); }

    private:
        void EnsureFormats();

        void DrawGrid(
            _In_ canvas::CanvasDrawingSession const& ds,
            _In_ float top,
            _In_ float height,
            _In_ float width,
            _In_ ArrangeDrawContext const& context,
            _In_ Color barColor,
            _In_ Color beatColor);

        void DrawTempoLane(_In_ canvas::CanvasDrawingSession const& ds, _In_ float top, _In_ float height, _In_ float width, _In_ ArrangeDrawContext const& context);
        void DrawTrackLane(_In_ canvas::CanvasDrawingSession const& ds, _In_ Track const& track, _In_ ArrangeRow const& row, _In_ float top, _In_ float width, _In_ ArrangeDrawContext const& context);
        void DrawFolderLane(_In_ canvas::CanvasDrawingSession const& ds, _In_ Track const& folder, _In_ float top, _In_ float height, _In_ float width, _In_ ArrangeDrawContext const& context);

        void DrawClipBlock(
            _In_ canvas::CanvasDrawingSession const& ds,
            _In_ Track const& track,
            _In_ Placement const& placement,
            _In_ bool selected,
            _In_ bool muted,
            _In_ float top,
            _In_ float height,
            _In_ float width,
            _In_ ArrangeDrawContext const& context);

        void DrawNotes(
            _In_ canvas::CanvasDrawingSession const& ds,
            _In_ Clip const& clip,
            _In_ std::vector<Note> const& notes,
            _In_ foundation::Rect const& area,
            _In_ int64_t firstPassTick,
            _In_ int64_t endTick,
            _In_ double pixelsPerTick,
            _In_ double areaStartTick,
            _In_ Color color,
            _In_ bool dashed);

        void DrawTag(
            _In_ canvas::CanvasDrawingSession const& ds,
            _In_ Tag const& tag,
            _In_ float x,
            _In_ float top,
            _In_ float height,
            _In_ bool atTop,
            _In_ ArrangeDrawContext const& context);

        void DrawCell(
            _In_ canvas::CanvasDrawingSession const& ds,
            _In_ Track const& track,
            _In_ size_t scene,
            _In_ foundation::Rect const& slot,
            _In_ bool muted,
            _In_ ArrangeDrawContext const& context);

        void DrawKindMark(_In_ canvas::CanvasDrawingSession const& ds, _In_ ClipKind kind, _In_ float x, _In_ float y, _In_ float size, _In_ Color color);

        std::pair<uint8_t, uint8_t> PitchRange(_In_ Clip const& clip, _In_ std::vector<Note> const& notes);

        canvasText::CanvasTextFormat m_caption{ nullptr };
        canvasText::CanvasTextFormat m_captionGlyph{ nullptr };
        canvasText::CanvasTextFormat m_rulerNumber{ nullptr };
        canvasText::CanvasTextFormat m_tagText{ nullptr };
        canvasText::CanvasTextFormat m_cellName{ nullptr };
        canvasText::CanvasTextFormat m_cellGlyph{ nullptr };
        canvasText::CanvasTextFormat m_slotGlyph{ nullptr };
        canvasText::CanvasTextFormat m_badge{ nullptr };
        canvasText::CanvasTextFormat m_tempoLabel{ nullptr };
        canvasText::CanvasTextFormat m_overlayText{ nullptr };
        canvasText::CanvasTextFormat m_overlayButton{ nullptr };

        std::unordered_map<std::wstring, std::pair<uint8_t, uint8_t>> m_pitchRanges{};

        float m_overlayTextWidth{ 130.0f };
        float m_overlayButtonWidth{ 110.0f };
    };

    // "Beat A x4": a clip's name, with how many times it plays when its placement loops it.
    std::wstring PlacementCaption(_In_ Clip const& clip, _In_ Placement const& placement);
}
