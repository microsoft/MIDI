// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The notes clip editor: a horizontal piano roll with the keys on the left, a ruler above and a
// velocity lane below, drawn with Win2D. It edits by gesture and hands each finished gesture back
// as notes taken out and put in, which the window turns into one undo step.

#include "AppSettings.h"
#include "SequenceModel.h"
#include "ThemePalette.h"

namespace midisequencer
{
    enum class RollTool : uint8_t
    {
        Select = 0,
        Draw = 1,
    };

    // What a gesture changed. The window applies it as one undo step.
    struct RollEdit
    {
        std::wstring Name{};
        std::vector<Note> Removed{};
        std::vector<Note> Added{};
    };

    struct RollStrings
    {
        winrt::hstring Velocity{};
        winrt::hstring Midi2Velocity{};     // "16-bit"
        winrt::hstring Midi1Velocity{};     // "7-bit"
        winrt::hstring PercentVelocity{};   // "Percent"
        winrt::hstring EmptyClip{};         // shown over an empty clip
    };

    class PianoRoll
    {
    public:
        static constexpr float RulerHeight = 22.0f;
        static constexpr float KeysWidth = 56.0f;
        static constexpr float LaneHeight = 54.0f;

        void SetClip(_In_opt_ Clip const* clip, _In_ Color color);
        void SetPalette(_In_ Palette const* palette) noexcept { m_palette = palette; }
        void SetMeter(_In_ std::vector<MeterChange> const* meter) noexcept { m_meter = meter; }
        void SetStrings(_In_ RollStrings strings) { m_strings = std::move(strings); }
        void SetSnap(_In_ int64_t ticks) noexcept { m_snap = ticks; }
        void SetTool(_In_ RollTool tool) noexcept { m_tool = tool; }
        void SetValuesAs(_In_ ValueDisplay value) noexcept { m_valuesAs = value; }

        // Where the playhead is, from the clip's start, or -1 to hide it.
        void SetPlayhead(_In_ int64_t tick) noexcept { m_playhead = tick; }

        RollTool Tool() const noexcept { return m_tool; }
        bool HasClip() const noexcept { return m_clip != nullptr; }

        void Draw(_In_ canvas::CanvasDrawingSession const& ds, _In_ float width, _In_ float height);

        // Shows the whole clip and centers its notes. Called when a clip opens and when asked.
        void FitToClip(_In_ float width, _In_ float height);

        // Pointer input, in pixels from the canvas's top left. Each returns true when the editor
        // needs drawing again.
        bool PointerPressed(_In_ foundation::Point point, _In_ bool shift, _In_ bool control, _In_ bool rightButton, _In_ bool doubleClick);
        bool PointerMoved(_In_ foundation::Point point);
        std::optional<RollEdit> PointerReleased(_In_ foundation::Point point);
        bool Wheel(_In_ foundation::Point point, _In_ int32_t delta, _In_ bool shift, _In_ bool control, _In_ float width, _In_ float height);

        // The key under the pointer, for hearing a note by clicking the keyboard.
        std::optional<uint8_t> KeyAt(_In_ foundation::Point point) const noexcept;

        // Keyboard editing.
        std::optional<RollEdit> DeleteSelection();
        std::optional<RollEdit> MoveSelection(_In_ int64_t ticks, _In_ int32_t semitones);
        std::optional<RollEdit> QuantizeSelection(_In_ int64_t grid);
        std::optional<RollEdit> TransposeSelection(_In_ int32_t semitones);
        void SelectAll();
        void ClearSelection() noexcept { m_selected.clear(); }

        std::vector<Note> const& Selection() const noexcept { return m_selected; }
        void SetSelection(_In_ std::vector<Note> notes);

        // The pointer shape the window should show at a point: true for the resize arrows.
        bool IsOverNoteEdge(_In_ foundation::Point point) const noexcept;

    private:
        enum class Gesture : uint8_t
        {
            None,
            Move,
            Resize,
            Draw,
            Band,
            Velocity,
            Scroll,
        };

        float RollTop() const noexcept { return RulerHeight; }
        float RollBottom() const noexcept { return m_height - LaneHeight - 1.0f; }
        float RollLeft() const noexcept { return KeysWidth; }

        double XAtTick(_In_ double tick) const noexcept { return RollLeft() + (tick - m_scrollTick) * m_pixelsPerTick; }
        double TickAtX(_In_ double x) const noexcept { return m_scrollTick + (x - RollLeft()) / m_pixelsPerTick; }
        double YAtNote(_In_ double note) const noexcept { return RollTop() + (m_topNote - note) * m_rowHeight; }
        int32_t NoteAtY(_In_ double y) const noexcept;

        // The note under a point, as an index into the clip, or -1.
        int64_t NoteIndexAt(_In_ foundation::Point point) const noexcept;
        int64_t VelocityStemAt(_In_ foundation::Point point) const noexcept;

        bool IsSelected(_In_ Note const& note) const noexcept;
        int64_t Snap(_In_ double tick) const noexcept;
        void ClampView() noexcept;

        void DrawRuler(_In_ canvas::CanvasDrawingSession const& ds);
        void DrawKeys(_In_ canvas::CanvasDrawingSession const& ds);
        void DrawGrid(_In_ canvas::CanvasDrawingSession const& ds, _In_ float top, _In_ float bottom, _In_ bool steps);
        void DrawNotes(_In_ canvas::CanvasDrawingSession const& ds);
        void DrawVelocityLane(_In_ canvas::CanvasDrawingSession const& ds);

        // Where a note is drawn while a gesture moves or stretches it.
        Note PreviewOf(_In_ Note const& note) const noexcept;

        void EnsureFormats();

        // Into the window's sequence, so only good until the next edit. The window sets it again
        // after every change.
        Clip const* m_clip{ nullptr };
        std::wstring m_clipId{};
        Color m_color{};
        Palette const* m_palette{ nullptr };
        std::vector<MeterChange> const* m_meter{ nullptr };
        RollStrings m_strings{};
        RollTool m_tool{ RollTool::Select };
        ValueDisplay m_valuesAs{ ValueDisplay::Midi2 };
        int64_t m_snap{ 240 };
        int64_t m_playhead{ -1 };

        float m_width{ 0 };
        float m_height{ 0 };

        double m_pixelsPerTick{ 0.1 };
        double m_scrollTick{ 0 };
        double m_rowHeight{ 8 };
        double m_topNote{ 72 };

        std::vector<Note> m_selected{};

        Gesture m_gesture{ Gesture::None };
        foundation::Point m_pressPoint{};
        foundation::Point m_lastPoint{};
        std::vector<Note> m_gestureNotes{};
        int64_t m_deltaTicks{ 0 };
        int32_t m_deltaNotes{ 0 };
        int64_t m_deltaLength{ 0 };
        Note m_drawing{};
        uint16_t m_dragVelocity{ 0 };
        bool m_bandAdds{ false };
        int64_t m_lastLength{ 240 };

        canvasText::CanvasTextFormat m_rulerText{ nullptr };
        canvasText::CanvasTextFormat m_keyText{ nullptr };
        canvasText::CanvasTextFormat m_laneTitle{ nullptr };
        canvasText::CanvasTextFormat m_laneSub{ nullptr };
        canvasText::CanvasTextFormat m_emptyText{ nullptr };
    };

    // "E1 (40)": the note name with C3 = 60, the family's convention, and its number.
    std::wstring NoteLabel(_In_ uint8_t number);

    // "2.1.000": bar, beat and tick, counted from 1 for bars and beats.
    std::wstring PositionLabel(_In_ std::vector<MeterChange> const& meter, _In_ int64_t tick);

    // "0.0.403": a length in bars, beats and ticks, counted from 0.
    std::wstring LengthLabel(_In_ int64_t ticks);
}
