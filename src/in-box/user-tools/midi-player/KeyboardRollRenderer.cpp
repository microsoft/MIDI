// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "KeyboardRollRenderer.h"
#include "NoteRollRenderer.h"

namespace composition = winrt::Microsoft::UI::Composition;

namespace midiplayer
{
    namespace
    {
        constexpr double MicrosecondsPerSecond = 1000000.0;

        constexpr uint32_t WhiteKeyColor = 0xFFEDEDED;
        constexpr uint32_t BlackKeyColor = 0xFF161616;
        constexpr uint32_t KeyboardEdgeColor = 0xFF8C3B2E;

        constexpr bool IsBlackKey(uint8_t note) noexcept
        {
            switch (note % 12)
            {
            case 1: case 3: case 6: case 8: case 10:
                return true;
            default:
                return false;
            }
        }

        winrt::Windows::UI::Color ColorFromArgb(uint32_t argb) noexcept
        {
            return winrt::Windows::UI::Color
            {
                static_cast<uint8_t>((argb >> 24) & 0xFF),
                static_cast<uint8_t>((argb >> 16) & 0xFF),
                static_cast<uint8_t>((argb >> 8) & 0xFF),
                static_cast<uint8_t>(argb & 0xFF)
            };
        }

        uint32_t ArgbFromColor(winrt::Windows::UI::Color const& color) noexcept
        {
            return (static_cast<uint32_t>(color.A) << 24)
                | (static_cast<uint32_t>(color.R) << 16)
                | (static_cast<uint32_t>(color.G) << 8)
                | static_cast<uint32_t>(color.B);
        }
    }

    _Use_decl_annotations_
    void KeyboardRollRenderer::Initialize(winrt::Microsoft::UI::Xaml::UIElement const& host) noexcept
    {
        try
        {
            auto const hostVisual = xaml::Hosting::ElementCompositionPreview::GetElementVisual(host);

            if (hostVisual == nullptr)
            {
                return;
            }

            m_compositor = hostVisual.Compositor();

            m_root = m_compositor.CreateContainerVisual();
            m_noteLayer = m_compositor.CreateContainerVisual();
            m_whiteKeyLayer = m_compositor.CreateContainerVisual();
            m_blackKeyLayer = m_compositor.CreateContainerVisual();

            // Notes first, then the keyboard over them, so a note disappears behind the keys
            // instead of sliding across them. Black keys sit above the white ones.
            m_root.Children().InsertAtTop(m_noteLayer);
            m_root.Children().InsertAtTop(m_whiteKeyLayer);
            m_root.Children().InsertAtTop(m_blackKeyLayer);

            xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(host, m_root);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to set up the keyboard display.")
    }

    void KeyboardRollRenderer::Shutdown() noexcept
    {
        try
        {
            m_notePool.clear();
            m_keyVisuals.clear();
            m_brushes.clear();
            m_keys.clear();
            m_litBy.clear();

            m_blackKeyLayer = nullptr;
            m_whiteKeyLayer = nullptr;
            m_noteLayer = nullptr;
            m_root = nullptr;
            m_compositor = nullptr;

            m_laidOutWidth = 0.0;
            m_laidOutKeyboardTop = -1.0;

            m_sequence.reset();
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to tear down the keyboard display.")
    }

    _Use_decl_annotations_
    void KeyboardRollRenderer::SetSequence(std::shared_ptr<midifile::MidiSequence const> const& sequence) noexcept
    {
        m_sequence = sequence;
        m_audible.clear();

        RebuildRange();

        // Force the keyboard to be laid out again for the new range.
        m_laidOutWidth = 0.0;
        m_laidOutKeyboardTop = -1.0;

        HideNotesFrom(0);
    }

    void KeyboardRollRenderer::RebuildRange() noexcept
    {
        int32_t low = 21;       // A0
        int32_t high = 108;     // C8

        if (m_sequence != nullptr && !m_sequence->Notes.empty())
        {
            low = static_cast<int32_t>(m_sequence->LowestNote);
            high = static_cast<int32_t>(m_sequence->HighestNote);

            // Two octaves is the least that still reads as a keyboard.
            while (high - low < 24)
            {
                if (low > 0) { --low; }
                if (high < 127) { ++high; }
            }
        }

        // The ends have to be white keys, or the first and last slots are half a key wide.
        while (low > 0 && IsBlackKey(static_cast<uint8_t>(low))) { --low; }
        while (high < 127 && IsBlackKey(static_cast<uint8_t>(high))) { ++high; }

        m_lowestNote = static_cast<uint8_t>(low < 0 ? 0 : low);
        m_highestNote = static_cast<uint8_t>(high > 127 ? 127 : high);
    }

    _Use_decl_annotations_
    void KeyboardRollRenderer::SetAudibleTracks(std::vector<bool> const& audible) noexcept
    {
        m_audible = audible;
    }

    _Use_decl_annotations_
    composition::CompositionColorBrush KeyboardRollRenderer::BrushFor(uint32_t argb) noexcept
    {
        auto const found = m_brushes.find(argb);

        if (found != m_brushes.end())
        {
            return found->second;
        }

        auto created = m_compositor.CreateColorBrush(ColorFromArgb(argb));

        // insert_or_assign, not operator[]: a WinRT type has no default constructor for the map
        // to build first.
        m_brushes.insert_or_assign(argb, created);

        return created;
    }

    _Use_decl_annotations_
    void KeyboardRollRenderer::LayOutKeyboard(double width) noexcept
    {
        m_keys.clear();

        auto const count = static_cast<size_t>(m_highestNote - m_lowestNote) + 1;

        m_keys.resize(count);
        m_litBy.assign(count, -1);

        uint32_t whiteCount = 0;

        for (uint8_t note = m_lowestNote; note <= m_highestNote; ++note)
        {
            if (!IsBlackKey(note)) { ++whiteCount; }

            if (note == 127) { break; }
        }

        if (whiteCount == 0)
        {
            return;
        }

        auto const whiteWidth = width / whiteCount;
        auto const blackWidth = whiteWidth * 0.62;

        uint32_t whitesSoFar = 0;

        for (uint8_t note = m_lowestNote; note <= m_highestNote; ++note)
        {
            auto& key = m_keys[static_cast<size_t>(note - m_lowestNote)];

            key.IsBlack = IsBlackKey(note);

            if (key.IsBlack)
            {
                // A black key straddles the join between the two white keys around it, and
                // whitesSoFar is exactly the index of that join.
                key.Left = whitesSoFar * whiteWidth - blackWidth / 2.0;
                key.Width = blackWidth;
            }
            else
            {
                key.Left = whitesSoFar * whiteWidth;
                key.Width = whiteWidth;

                ++whitesSoFar;
            }

            if (note == 127) { break; }
        }
    }

    _Use_decl_annotations_
    void KeyboardRollRenderer::DrawKeyboard(double keyboardTop, double keyboardHeight) noexcept
    {
        // White keys run the full depth, black keys about two thirds, which is what makes the
        // shape read as a keyboard rather than a bar chart.
        auto const blackHeight = keyboardHeight * 0.62;

        m_keyVisuals.clear();
        m_whiteKeyLayer.Children().RemoveAll();
        m_blackKeyLayer.Children().RemoveAll();

        m_keyVisuals.resize(m_keys.size(), nullptr);

        for (size_t pass = 0; pass < 2; ++pass)
        {
            auto const wantBlack = pass == 1;

            for (size_t index = 0; index < m_keys.size(); ++index)
            {
                auto const& key = m_keys[index];

                if (key.IsBlack != wantBlack)
                {
                    continue;
                }

                auto visual = m_compositor.CreateSpriteVisual();

                visual.Offset({ static_cast<float>(key.Left), static_cast<float>(keyboardTop), 0.0f });

                // A hairline gap keeps neighbouring white keys apart without drawing separators.
                visual.Size({
                    static_cast<float>(key.Width - (wantBlack ? 0.0 : 1.0)),
                    static_cast<float>(wantBlack ? blackHeight : keyboardHeight) });

                visual.Brush(BrushFor(wantBlack ? BlackKeyColor : WhiteKeyColor));

                (wantBlack ? m_blackKeyLayer : m_whiteKeyLayer).Children().InsertAtTop(visual);

                m_keyVisuals[index] = visual;
            }
        }

        // The felt strip along the top of a real keyboard, and a useful playhead line.
        auto edge = m_compositor.CreateSpriteVisual();

        edge.Offset({ 0.0f, static_cast<float>(keyboardTop - 2.0), 0.0f });
        edge.Size({ static_cast<float>(m_laidOutWidth), 2.0f });
        edge.Brush(BrushFor(KeyboardEdgeColor));

        m_whiteKeyLayer.Children().InsertAtBottom(edge);
    }

    _Use_decl_annotations_
    void KeyboardRollRenderer::Render(uint64_t positionMicroseconds, double width, double height) noexcept
    {
        try
        {
            if (m_root == nullptr || width <= 0.0 || height <= 0.0)
            {
                return;
            }

            m_root.Size({ static_cast<float>(width), static_cast<float>(height) });

            auto keyboardHeight = height * KeyboardHeightFraction;

            keyboardHeight = keyboardHeight < MinimumKeyboardHeight ? MinimumKeyboardHeight : keyboardHeight;
            keyboardHeight = keyboardHeight > MaximumKeyboardHeight ? MaximumKeyboardHeight : keyboardHeight;
            keyboardHeight = keyboardHeight > height ? height : keyboardHeight;

            auto const keyboardTop = height - keyboardHeight;

            if (width != m_laidOutWidth || keyboardTop != m_laidOutKeyboardTop ||
                keyboardHeight != m_laidOutKeyboardHeight)
            {
                m_laidOutWidth = width;
                m_laidOutKeyboardTop = keyboardTop;
                m_laidOutKeyboardHeight = keyboardHeight;

                LayOutKeyboard(width);
                DrawKeyboard(keyboardTop, keyboardHeight);
            }

            if (m_sequence == nullptr || m_sequence->Notes.empty() || m_keys.empty())
            {
                HideNotesFrom(0);
                return;
            }

            std::fill(m_litBy.begin(), m_litBy.end(), -1);

            auto const position = static_cast<double>(positionMicroseconds) / MicrosecondsPerSecond;
            auto const windowEnd = position + SecondsAhead;

            auto const startTick = m_sequence->TickAtMicroseconds(positionMicroseconds);
            auto const endTick = m_sequence->TickAtMicroseconds(static_cast<uint64_t>(windowEnd * MicrosecondsPerSecond));

            // Notes are ordered by start, so one that began before now can still be sounding.
            auto const searchTick = startTick > m_sequence->LongestNoteTicks
                ? startTick - m_sequence->LongestNoteTicks
                : 0u;

            auto const first = std::lower_bound(
                m_sequence->Notes.begin(),
                m_sequence->Notes.end(),
                searchTick,
                [](midifile::Note const& note, uint32_t value) noexcept { return note.StartTick < value; });

            size_t used = 0;

            for (auto entry = first; entry != m_sequence->Notes.end(); ++entry)
            {
                if (entry->StartTick > endTick)
                {
                    break;
                }

                if (entry->NoteNumber < m_lowestNote || entry->NoteNumber > m_highestNote)
                {
                    continue;
                }

                auto const noteStart = static_cast<double>(m_sequence->MicrosecondsAtTick(entry->StartTick))
                    / MicrosecondsPerSecond;

                auto const noteEnd = static_cast<double>(m_sequence->MicrosecondsAtTick(entry->EndTick))
                    / MicrosecondsPerSecond;

                if (noteEnd < position)
                {
                    continue;
                }

                auto const keyIndex = static_cast<size_t>(entry->NoteNumber - m_lowestNote);
                auto const& key = m_keys[keyIndex];

                auto const audible = entry->TrackIndex >= m_audible.size() || m_audible[entry->TrackIndex];

                if (noteStart <= position && audible)
                {
                    m_litBy[keyIndex] = static_cast<int32_t>(entry->TrackIndex);
                }

                if (used >= MaximumVisibleNotes)
                {
                    continue;
                }

                // Time runs up the display: later music is higher, and a note meets the keyboard
                // at the moment it sounds.
                auto const bottom = keyboardTop * (1.0 - (noteStart - position) / SecondsAhead);
                auto const top = keyboardTop * (1.0 - (noteEnd - position) / SecondsAhead);

                auto const clampedBottom = bottom > keyboardTop ? keyboardTop : bottom;
                auto const clampedTop = top < 0.0 ? 0.0 : top;

                if (clampedBottom <= 0.0 || clampedTop >= keyboardTop)
                {
                    continue;
                }

                auto visual = TakeNoteVisual(used);

                if (visual == nullptr)
                {
                    break;
                }

                auto const barHeight = clampedBottom - clampedTop;

                visual.Offset({ static_cast<float>(key.Left), static_cast<float>(clampedTop), 0.0f });
                visual.Size({
                    static_cast<float>(key.Width > 2.0 ? key.Width - 1.0 : key.Width),
                    static_cast<float>(barHeight < 2.0 ? 2.0 : barHeight) });

                auto color = NoteRollRenderer::TrackColor(entry->TrackIndex);

                if (!audible)
                {
                    color.A = 48;
                }
                else if (key.IsBlack)
                {
                    // Black key notes are drawn a shade darker so the two rows stay tellable
                    // apart where they overlap.
                    color.R = static_cast<uint8_t>(color.R * 0.72);
                    color.G = static_cast<uint8_t>(color.G * 0.72);
                    color.B = static_cast<uint8_t>(color.B * 0.72);
                }

                visual.Brush(BrushFor(ArgbFromColor(color)));
                visual.IsVisible(true);

                ++used;
            }

            HideNotesFrom(used);

            for (size_t index = 0; index < m_keyVisuals.size(); ++index)
            {
                auto const visual = m_keyVisuals[index];

                if (visual == nullptr)
                {
                    continue;
                }

                auto const track = m_litBy[index];

                if (track < 0)
                {
                    visual.Brush(BrushFor(m_keys[index].IsBlack ? BlackKeyColor : WhiteKeyColor));
                    continue;
                }

                visual.Brush(BrushFor(ArgbFromColor(
                    NoteRollRenderer::TrackColor(static_cast<uint16_t>(track)))));
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to draw the keyboard display.")
    }

    _Use_decl_annotations_
    composition::SpriteVisual KeyboardRollRenderer::TakeNoteVisual(size_t index) noexcept
    {
        if (index < m_notePool.size())
        {
            return m_notePool[index];
        }

        if (m_compositor == nullptr || m_noteLayer == nullptr)
        {
            return nullptr;
        }

        auto visual = m_compositor.CreateSpriteVisual();

        m_noteLayer.Children().InsertAtTop(visual);
        m_notePool.push_back(visual);

        return visual;
    }

    _Use_decl_annotations_
    void KeyboardRollRenderer::HideNotesFrom(size_t index) noexcept
    {
        for (auto entry = index; entry < m_notePool.size(); ++entry)
        {
            m_notePool[entry].IsVisible(false);
        }
    }
}
