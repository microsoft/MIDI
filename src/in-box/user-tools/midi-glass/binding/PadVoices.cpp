// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK.

#include "PadVoices.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        void Append(
            _Inout_ std::span<PadAction> actions,
            _Inout_ uint32_t& written,
            _In_ PadActionKind kind,
            _In_ uint8_t note,
            _In_ double value) noexcept
        {
            if (written >= actions.size())
            {
                return;
            }

            actions[written++] = PadAction{ kind, note, value };
        }

        bool IsNote(_In_ int32_t note) noexcept
        {
            return note >= 0 && note <= 127;
        }

        double HitFrom(_In_ double velocity) noexcept
        {
            return std::isfinite(velocity) ? std::clamp(velocity, 0.0, 1.0) : 1.0;
        }
    }

    _Use_decl_annotations_
    void PadVoices::Configure(PadGlide glide, int32_t bendRangeSemitones) noexcept
    {
        m_glide = glide;
        m_bendRange = std::clamp(bendRangeSemitones, MinimumBendRangeSemitones, MaximumBendRangeSemitones);
    }

    _Use_decl_annotations_
    PadVoices::Voice* PadVoices::Find(uint32_t touch) noexcept
    {
        for (auto& voice : m_voices)
        {
            if (voice.InUse && voice.Touch == touch)
            {
                return &voice;
            }
        }

        return nullptr;
    }

    size_t PadVoices::HeldCount() const noexcept
    {
        size_t held{ 0 };

        for (auto const& voice : m_voices)
        {
            if (voice.InUse)
            {
                held++;
            }
        }

        return held;
    }

    _Use_decl_annotations_
    void PadVoices::LetGo(Voice& voice, std::span<PadAction> actions, uint32_t& written) noexcept
    {
        auto const note = voice.Note;

        if (m_holders[note] > 0)
        {
            m_holders[note]--;
        }

        if (m_holders[note] == 0)
        {
            Append(actions, written, PadActionKind::NoteOff, note, 0.0);
        }
        else if (voice.Owns)
        {
            // Somebody else is still holding it, so it keeps sounding and they own it now,
            // bend and all.
            for (auto& other : m_voices)
            {
                if (&other != &voice && other.InUse && other.Note == note)
                {
                    other.Owns = true;
                    other.Bend = voice.Bend;
                    break;
                }
            }
        }

        voice.Owns = false;
    }

    _Use_decl_annotations_
    uint32_t PadVoices::Press(
        uint32_t touch,
        int32_t note,
        double velocity,
        double pitch,
        std::span<PadAction> actions) noexcept
    {
        uint32_t written{ 0 };

        if (!IsNote(note))
        {
            return 0;
        }

        // A press from a finger that is somehow already down is that finger starting again.
        if (auto* const stale = Find(touch))
        {
            LetGo(*stale, actions, written);
            stale->InUse = false;
        }

        Voice* voice{ nullptr };

        for (auto& candidate : m_voices)
        {
            if (!candidate.InUse)
            {
                voice = &candidate;
                break;
            }
        }

        if (voice == nullptr)
        {
            return written;
        }

        auto const key = static_cast<uint8_t>(note);

        voice->InUse = true;
        voice->Touch = touch;
        voice->Note = key;
        voice->Bend = 0.0;
        voice->Owns = m_holders[key] == 0;

        m_holders[key]++;

        if (!voice->Owns)
        {
            return written;
        }

        if (m_glide == PadGlide::PerNoteBend)
        {
            // How far a bend reaches, then the note put back where the finger is, whatever the
            // last finger on this note left its bend at, and only then the note itself. A
            // per-note bend outlives the note it was sent for.
            auto const bend = std::isfinite(pitch)
                ? std::clamp(pitch - note, -static_cast<double>(m_bendRange), static_cast<double>(m_bendRange))
                : 0.0;

            Append(actions, written, PadActionKind::BendRange, key, static_cast<double>(m_bendRange));
            Append(actions, written, PadActionKind::Bend, key, bend);

            voice->Bend = bend;
        }

        Append(actions, written, PadActionKind::NoteOn, key, HitFrom(velocity));

        return written;
    }

    _Use_decl_annotations_
    uint32_t PadVoices::Move(
        uint32_t touch,
        int32_t note,
        double velocity,
        double pitch,
        std::span<PadAction> actions) noexcept
    {
        uint32_t written{ 0 };

        auto* const voice = Find(touch);

        if (voice == nullptr)
        {
            return 0;
        }

        if (m_glide == PadGlide::PerNoteBend)
        {
            // The note does not change; how far it is bent does. A finger that joined a note
            // somebody else started cannot bend it out from under them.
            if (!voice->Owns || !std::isfinite(pitch))
            {
                return 0;
            }

            auto const bend = std::clamp(
                pitch - voice->Note,
                -static_cast<double>(m_bendRange),
                static_cast<double>(m_bendRange));

            if (std::abs(bend - voice->Bend) < PadBendStep)
            {
                return 0;
            }

            voice->Bend = bend;

            Append(actions, written, PadActionKind::Bend, voice->Note, bend);

            return written;
        }

        // Off the grid, or still on the same pad: nothing has changed.
        if (!IsNote(note) || note == voice->Note)
        {
            return 0;
        }

        auto const from = voice->Note;
        auto const to = static_cast<uint8_t>(note);

        // A glide moves one voice from one key to another, so it only works when this finger is
        // the only one holding the note it is leaving and nobody is already holding the one it
        // is arriving at.
        auto const glides = m_glide == PadGlide::Portamento &&
            voice->Owns &&
            m_holders[from] == 1 &&
            m_holders[to] == 0;

        if (glides)
        {
            // Named, started, then ended. An instrument that follows control change 84 glides the
            // one voice across, and the note off for the old key finds nothing still keyed to it.
            // One that ignores it hears two notes overlap, which is exactly what a mono synth set
            // to legato glides on anyway.
            Append(actions, written, PadActionKind::PortamentoFrom, from, 0.0);
            Append(actions, written, PadActionKind::NoteOn, to, HitFrom(velocity));
            Append(actions, written, PadActionKind::NoteOff, from, 0.0);

            m_holders[from] = 0;
            m_holders[to] = 1;

            voice->Note = to;
            voice->Owns = true;

            return written;
        }

        // Off before on, so a slide never leaves a note sounding behind the finger.
        LetGo(*voice, actions, written);

        voice->Note = to;
        voice->Bend = 0.0;
        voice->Owns = m_holders[to] == 0;

        m_holders[to]++;

        if (voice->Owns)
        {
            Append(actions, written, PadActionKind::NoteOn, to, HitFrom(velocity));
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t PadVoices::Release(uint32_t touch, std::span<PadAction> actions) noexcept
    {
        uint32_t written{ 0 };

        auto* const voice = Find(touch);

        if (voice == nullptr)
        {
            return 0;
        }

        LetGo(*voice, actions, written);

        voice->InUse = false;

        return written;
    }

    _Use_decl_annotations_
    uint32_t PadVoices::ReleaseAll(std::span<PadAction> actions) noexcept
    {
        uint32_t written{ 0 };

        for (size_t note = 0; note < m_holders.size(); ++note)
        {
            if (m_holders[note] > 0)
            {
                Append(actions, written, PadActionKind::NoteOff, static_cast<uint8_t>(note), 0.0);
            }
        }

        m_holders.fill(0);

        for (auto& voice : m_voices)
        {
            voice = Voice{};
        }

        return written;
    }
}
