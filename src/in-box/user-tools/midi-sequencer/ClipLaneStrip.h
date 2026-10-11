// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A notes clip's own automation, in lanes under the piano roll that share its time scale
// (comp 2). They're edited the way a track's lanes are on the timeline: click to add a point,
// drag a point to move it, and hold Ctrl and drag to draw. A gesture works on a copy and comes
// back as the lanes before and after, which the window turns into one undo step.

#include "PianoRoll.h"

namespace midisequencer
{
    struct ClipLaneEdit
    {
        std::vector<AutomationLane> Before{};
        std::vector<AutomationLane> After{};

        // Drawn with the pencil, rather than a point added or moved.
        bool Drawn{ false };
    };

    struct ClipLaneStrings
    {
        winrt::hstring AddLane{};       // the button under the lanes
        winrt::hstring EmptyLane{};     // over a lane with no points
        winrt::hstring NoLanes{};       // beside the button when there are none
    };

    // A lane's name, and what it changes, short enough for the narrow column on the left.
    struct ClipLaneLabel
    {
        winrt::hstring Name{};
        winrt::hstring Target{};
    };

    // What's under the pointer.
    struct ClipLaneHit
    {
        enum class Kind : uint8_t { None, AddButton, Header, Lane, Point };

        Kind What{ Kind::None };
        size_t Lane{ SIZE_MAX };
        size_t Point{ SIZE_MAX };

        // Where on the lane, from the clip's start, snapped, and the value there.
        int64_t Tick{ 0 };
        uint32_t Value{ 0 };
    };

    class ClipLaneStrip
    {
    public:
        static constexpr float RowHeight = 48.0f;
        static constexpr float AddRowHeight = 26.0f;

        void SetClip(_In_opt_ Clip const* clip, _In_ Color color);
        void SetPalette(_In_ Palette const* palette) noexcept { m_palette = palette; }
        void SetStrings(_In_ ClipLaneStrings strings) { m_strings = std::move(strings); }
        void SetLabels(_In_ std::vector<ClipLaneLabel> labels) { m_labels = std::move(labels); }

        // A row for each lane and one for the button.
        float Height() const noexcept;

        // Draws from top down, as wide as the canvas, on the roll's time scale.
        void Draw(_In_ canvas::CanvasDrawingSession const& ds, _In_ float top, _In_ float width, _In_ PianoRoll const& roll);

        ClipLaneHit HitTest(_In_ foundation::Point point, _In_ PianoRoll const& roll) const noexcept;

        // A press on a lane starts a gesture: picking up a point, adding one, or drawing with
        // pencil. Each returns true when the strip needs drawing again.
        bool PointerPressed(_In_ foundation::Point point, _In_ bool pencil, _In_ PianoRoll const& roll);
        bool PointerMoved(_In_ foundation::Point point, _In_ PianoRoll const& roll);
        std::optional<ClipLaneEdit> PointerReleased(_In_ foundation::Point point, _In_ PianoRoll const& roll);
        bool InGesture() const noexcept { return m_gesture != Gesture::None; }

        // The selected point, which Delete removes.
        void Select(_In_ size_t lane, _In_ size_t point) noexcept;
        bool HasSelection() const noexcept { return m_selectedLane != SIZE_MAX && m_selectedPoint != SIZE_MAX; }
        size_t SelectedPoint() const noexcept { return m_selectedPoint; }
        void ClearSelection() noexcept;
        std::optional<ClipLaneEdit> DeleteSelection();

        // The lane the keyboard works on, outlined while the keyboard is in use.
        size_t CurrentLane() const noexcept;
        void SetCurrentLane(_In_ size_t lane) noexcept;
        void HideKeyboardFocus() noexcept { m_keyboardFocus = false; }

        // Where to open a menu for a lane or for one of its points.
        foundation::Point LaneAnchor(_In_ size_t lane) const noexcept;
        foundation::Point PointPosition(_In_ size_t lane, _In_ size_t point, _In_ PianoRoll const& roll) const noexcept;

        // True for the first SetClip after the clip gained or lost a lane.
        bool LaneCountChanged() const noexcept { return m_laneCountChanged; }

    private:
        enum class Gesture : uint8_t
        {
            None,
            Move,
            Pencil,
        };

        float LaneTop(_In_ size_t lane) const noexcept { return m_top + RowHeight * static_cast<float>(lane); }
        float AddRowTop() const noexcept;
        foundation::Rect AddButtonBounds() const noexcept;
        std::vector<AutomationLane> const& ShownLanes() const noexcept;
        int64_t SnapTick(_In_ double tick, _In_ PianoRoll const& roll) const noexcept;
        size_t HitPoint(_In_ AutomationLane const& lane, _In_ float top, _In_ foundation::Point point, _In_ PianoRoll const& roll) const noexcept;

        void DrawLane(_In_ canvas::CanvasDrawingSession const& ds, _In_ AutomationLane const& lane, _In_ size_t index, _In_ float top, _In_ PianoRoll const& roll);
        void EnsureFormats();

        // Into the window's sequence, so only good until the next edit. The window sets it again
        // before every draw.
        Clip const* m_clip{ nullptr };
        std::wstring m_clipId{};
        Color m_color{};
        Palette const* m_palette{ nullptr };
        ClipLaneStrings m_strings{};
        std::vector<ClipLaneLabel> m_labels{};

        float m_top{ 0 };
        float m_width{ 0 };

        Gesture m_gesture{ Gesture::None };
        foundation::Point m_pressPoint{};
        bool m_moved{ false };
        size_t m_lane{ SIZE_MAX };
        size_t m_point{ SIZE_MAX };

        // The lanes as the gesture found them, and as it has changed them so far.
        std::vector<AutomationLane> m_before{};
        std::vector<AutomationLane> m_working{};

        // What the pencil has drawn, by tick, and where it last was.
        std::map<int64_t, uint32_t> m_drawn{};
        int64_t m_lastTick{ -1 };
        uint32_t m_lastValue{ 0 };

        size_t m_selectedLane{ SIZE_MAX };
        size_t m_selectedPoint{ SIZE_MAX };
        size_t m_currentLane{ 0 };
        bool m_keyboardFocus{ false };
        size_t m_laneCount{ 0 };
        bool m_laneCountChanged{ false };

        canvasText::CanvasTextFormat m_titleText{ nullptr };
        canvasText::CanvasTextFormat m_subText{ nullptr };
        canvasText::CanvasTextFormat m_buttonText{ nullptr };
        canvasText::CanvasTextFormat m_hintText{ nullptr };
    };
}
