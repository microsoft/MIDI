// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "midi_file_sequence.h"

namespace midiplayer
{
    // An alternate view of the same sequence: notes fall towards a piano keyboard and the keys
    // light up as they are played. This is a second renderer rather than a mode inside
    // NoteRollRenderer, because the two disagree about which axis carries time and sharing the
    // code would leave neither of them simple.
    //
    // Drawn with composition visuals and a fixed pool, for the same reason as the note roll: a
    // file can hold millions of notes and only what is on screen may cost anything.
    class KeyboardRollRenderer
    {
    public:
        void Initialize(_In_ winrt::Microsoft::UI::Xaml::UIElement const& host) noexcept;
        void Shutdown() noexcept;

        void SetSequence(_In_ std::shared_ptr<midifile::MidiSequence const> const& sequence) noexcept;
        void SetAudibleTracks(_In_ std::vector<bool> const& audible) noexcept;

        void Render(uint64_t positionMicroseconds, double width, double height) noexcept;

        // How far ahead the falling notes are visible.
        static constexpr double SecondsAhead = 4.0;

    private:
        struct KeyShape
        {
            double Left{ 0.0 };
            double Width{ 0.0 };
            bool IsBlack{ false };
        };

        void RebuildRange() noexcept;
        void LayOutKeyboard(double width) noexcept;
        void DrawKeyboard(double keyboardTop, double keyboardHeight) noexcept;

        winrt::Microsoft::UI::Composition::SpriteVisual TakeNoteVisual(size_t index) noexcept;
        void HideNotesFrom(size_t index) noexcept;

        winrt::Microsoft::UI::Composition::CompositionColorBrush BrushFor(uint32_t argb) noexcept;

        winrt::Microsoft::UI::Composition::Compositor m_compositor{ nullptr };
        winrt::Microsoft::UI::Composition::ContainerVisual m_root{ nullptr };
        winrt::Microsoft::UI::Composition::ContainerVisual m_noteLayer{ nullptr };
        winrt::Microsoft::UI::Composition::ContainerVisual m_whiteKeyLayer{ nullptr };
        winrt::Microsoft::UI::Composition::ContainerVisual m_blackKeyLayer{ nullptr };

        std::vector<winrt::Microsoft::UI::Composition::SpriteVisual> m_notePool{};

        // One visual per key, held for the life of the view and recolored as keys are played.
        std::vector<winrt::Microsoft::UI::Composition::SpriteVisual> m_keyVisuals{};

        std::map<uint32_t, winrt::Microsoft::UI::Composition::CompositionColorBrush> m_brushes{};

        std::shared_ptr<midifile::MidiSequence const> m_sequence{};
        std::vector<bool> m_audible{};

        // Geometry for the notes currently in range, indexed by note number minus m_lowestNote.
        std::vector<KeyShape> m_keys{};

        // Which track is sounding each key this frame, or -1. Reused so the frame path does not
        // allocate.
        std::vector<int32_t> m_litBy{};

        uint8_t m_lowestNote{ 21 };
        uint8_t m_highestNote{ 108 };

        double m_laidOutWidth{ 0.0 };
        double m_laidOutKeyboardTop{ -1.0 };
        double m_laidOutKeyboardHeight{ 0.0 };

        static constexpr size_t MaximumVisibleNotes = 2048;

        // The keyboard takes a share of the height, held between these so it stays playable
        // looking in a short window and does not swallow a tall one.
        static constexpr double KeyboardHeightFraction = 0.22;
        static constexpr double MinimumKeyboardHeight = 44.0;
        static constexpr double MaximumKeyboardHeight = 130.0;
    };
}
