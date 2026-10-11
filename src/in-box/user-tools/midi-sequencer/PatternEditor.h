// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The pattern clip editor (comp 3): a grid of steps with a lane for each drum, or rows for the
// notes, accents and slides of a melodic pattern, drawn with Win2D. Like the piano roll, it hands
// each finished gesture back as the pattern after it, which the window turns into one undo step.

#include "SequenceModel.h"
#include "SequencePattern.h"
#include "ThemePalette.h"

namespace midisequencer
{
    // What's under the pointer.
    enum class PatternTarget : uint8_t
    {
        None = 0,
        Step = 1,           // a step in a drum lane, or a note in a melodic pattern
        Accent = 2,
        Slide = 3,
        LaneName = 4,
        LaneMute = 5,
        LaneSolo = 6,
        AddLane = 7,
    };

    struct PatternHit
    {
        PatternTarget Target{ PatternTarget::None };
        size_t Lane{ 0 };
        uint16_t Step{ 0 };
    };

    // What a gesture changed. The window applies it as one undo step.
    struct PatternEdit
    {
        std::wstring Name{};
        PatternSettings After{};
    };

    struct PatternStrings
    {
        winrt::hstring DrumLanes{};         // "Drum lanes"
        winrt::hstring MelodicSteps{};      // "Steps"
        winrt::hstring AddLane{};           // "Add a lane"
        winrt::hstring NoteRow{};           // "Note"
        winrt::hstring NoteRowHint{};       // "Click a step, then use the arrow keys"
        winrt::hstring AccentRow{};         // "Accent"
        winrt::hstring SlideRow{};          // "Slide"
        winrt::hstring MuteLetter{};        // "M"
        winrt::hstring SoloLetter{};        // "S"
        winrt::hstring BadgeFirst{};        // a step that plays on the first pass only
        winrt::hstring BadgeNotFirst{};     // a step that plays on every pass but the first
        winrt::hstring UndoSteps{};
        winrt::hstring UndoVelocity{};

        // A lane's name, its note and length ("C1 (36), 16 steps"), and where it's playing, which
        // need resources.
        std::function<std::wstring(PatternLane const&)> LaneName{};
        std::function<std::wstring(PatternLane const&, uint16_t)> LaneDetail{};
        std::function<std::wstring(uint64_t, uint64_t)> Playing{};
    };

    class PatternEditor
    {
    public:
        static constexpr float TitleHeight = 30.0f;
        static constexpr float NumbersHeight = 18.0f;
        static constexpr float FooterHeight = 26.0f;
        static constexpr float LaneHeaderWidth = 188.0f;
        static constexpr float LaneHeight = 44.0f;
        static constexpr float SmallRowHeight = 30.0f;
        static constexpr float RowGap = 6.0f;
        static constexpr float CellGap = 4.0f;
        static constexpr float GroupGap = 12.0f;

        void SetClip(_In_opt_ Clip const* clip, _In_ Color color);
        void SetPalette(_In_ Palette const* palette) noexcept { m_palette = palette; }
        void SetStrings(_In_ PatternStrings strings) { m_strings = std::move(strings); }
        void SetFooter(_In_ std::wstring left, _In_ std::wstring middle, _In_ std::wstring right);

        // Where the clip is playing, from its start, and which pass through it. -1 hides it.
        void SetPlayhead(_In_ int64_t tick, _In_ uint64_t pass) noexcept;

        bool HasClip() const noexcept { return m_clip != nullptr; }

        void Draw(_In_ canvas::CanvasDrawingSession const& ds, _In_ float width, _In_ float height);

        PatternHit HitTest(_In_ foundation::Point point) const noexcept;

        // In the grid, a click turns a step on or off, and a drag up or down on a step that's on
        // sets its velocity. A right click only selects. Returns true when it needs drawing again.
        bool PointerPressed(_In_ foundation::Point point, _In_ bool rightButton);
        bool PointerMoved(_In_ foundation::Point point);
        std::optional<PatternEdit> PointerReleased(_In_ foundation::Point point);
        void CancelGesture() noexcept;
        bool Wheel(_In_ int32_t delta);

        // The selected step, which the inspector shows. It doesn't have to be on.
        bool HasSelection() const noexcept;
        size_t SelectedLane() const noexcept { return m_selectedLane; }
        uint16_t SelectedStep() const noexcept { return m_selectedStep; }
        void Select(_In_ size_t lane, _In_ uint16_t step) noexcept;
        void ClearSelection() noexcept { m_selectedLane = SIZE_MAX; }

        // Arrow keys. False when the selection is already at the edge, or there is none.
        bool MoveSelection(_In_ int32_t steps, _In_ int32_t lanes) noexcept;

        // The note a melodic step gets when it's turned on: the last one used.
        uint8_t NextNote() const noexcept { return m_nextNote; }
        void SetNextNote(_In_ uint8_t note) noexcept { m_nextNote = static_cast<uint8_t>(note & 0x7F); }

    private:
        enum class Gesture : uint8_t
        {
            None,
            Step,
            Velocity,
            Flag,
        };

        bool Melodic() const noexcept;
        uint16_t Columns() const noexcept;
        size_t RowCount() const noexcept;
        float RowHeight(_In_ size_t row) const noexcept;
        float RowTop(_In_ size_t row) const noexcept;
        float GridTop() const noexcept { return TitleHeight + NumbersHeight; }
        float GridBottom() const noexcept { return std::max(GridTop(), m_height - FooterHeight); }
        float ContentHeight() const noexcept;
        float ColumnX(_In_ uint16_t column) const noexcept;
        void ComputeColumns() noexcept;
        void ClampScroll() noexcept;
        void EnsureVisible(_In_ size_t row) noexcept;
        uint64_t PlayingGlobalStep() const noexcept;

        void DrawLaneHeader(_In_ canvas::CanvasDrawingSession const& ds, _In_ float top, _In_ float height, _In_ Color bar,
            _In_ std::wstring const& name, _In_ std::wstring const& detail, _In_opt_ PatternLane const* lane);
        void DrawCell(_In_ canvas::CanvasDrawingSession const& ds, _In_ float x, _In_ float y, _In_ float w, _In_ float h,
            _In_opt_ PatternStep const* step, _In_ bool pastEnd, _In_ bool selected, _In_ bool playing, _In_ std::wstring const& label);
        void DrawFlagCell(_In_ canvas::CanvasDrawingSession const& ds, _In_ float x, _In_ float y, _In_ float w, _In_ float h,
            _In_ bool on, _In_ Color color, _In_ bool selected, _In_ bool playing);

        void EnsureFormats();

        // Into the window's sequence, so only good until the next edit. The window sets it again
        // after every change.
        Clip const* m_clip{ nullptr };
        std::wstring m_clipId{};
        Color m_color{};
        Palette const* m_palette{ nullptr };
        PatternStrings m_strings{};
        std::wstring m_footerLeft{};
        std::wstring m_footerMiddle{};
        std::wstring m_footerRight{};

        int64_t m_playTick{ -1 };
        uint64_t m_playPass{ 0 };

        float m_width{ 0 };
        float m_height{ 0 };
        float m_scrollY{ 0 };
        float m_cellWidth{ 24 };
        uint16_t m_columns{ 16 };

        size_t m_selectedLane{ SIZE_MAX };
        uint16_t m_selectedStep{ 0 };
        uint8_t m_nextNote{ 48 };

        Gesture m_gesture{ Gesture::None };
        PatternHit m_pressHit{};
        foundation::Point m_pressPoint{};
        uint16_t m_startVelocity{ 0 };
        uint16_t m_dragVelocity{ 0 };
        bool m_pressedOn{ false };

        canvasText::CanvasTextFormat m_titleText{ nullptr };
        canvasText::CanvasTextFormat m_numberText{ nullptr };
        canvasText::CanvasTextFormat m_laneTitle{ nullptr };
        canvasText::CanvasTextFormat m_laneSub{ nullptr };
        canvasText::CanvasTextFormat m_badgeText{ nullptr };
        canvasText::CanvasTextFormat m_badgeRight{ nullptr };
        canvasText::CanvasTextFormat m_letterText{ nullptr };
        canvasText::CanvasTextFormat m_footerText{ nullptr };
        canvasText::CanvasTextFormat m_footerCenter{ nullptr };
        canvasText::CanvasTextFormat m_footerRightText{ nullptr };
        canvasText::CanvasTextFormat m_centerText{ nullptr };
    };
}
