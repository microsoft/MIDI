// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK, like PadVoices.

#include "StringVoices.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        // The smallest change in a bend worth sending: one cent.
        constexpr double StringBendStep = 0.01;

        double HitFrom(_In_ double velocity) noexcept
        {
            return std::isfinite(velocity) ? std::clamp(velocity, 0.0, 1.0) : 1.0;
        }
    }

    _Use_decl_annotations_
    void StringVoices::Configure(FretboardSpec const& spec)
    {
        m_spec = spec;
    }

    _Use_decl_annotations_
    void StringVoices::Append(
        std::span<StringAction> actions,
        uint32_t& written,
        StringActionKind kind,
        int32_t string,
        uint8_t note,
        double value) const noexcept
    {
        if (written < actions.size())
        {
            actions[written++] = StringAction{ kind, static_cast<uint8_t>(string), note, value };
        }
    }

    _Use_decl_annotations_
    bool StringVoices::OtherStringSounds(int32_t string, uint8_t note) const noexcept
    {
        if (m_spec.Output != FretOutput::PerNoteBend)
        {
            return false;
        }

        for (size_t index = 0; index < m_strings.size(); ++index)
        {
            if (static_cast<int32_t>(index) != string && m_strings[index].Sounding && m_strings[index].Note == note)
            {
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    StringVoices::Finger const* StringVoices::Active(StringState const& state) const noexcept
    {
        Finger const* best{ nullptr };

        for (size_t index = 0; index < state.FingerCount; ++index)
        {
            auto const& finger = state.Fingers[index];

            if (best == nullptr || finger.Fret > best->Fret || (finger.Fret == best->Fret && finger.Order > best->Order))
            {
                best = &finger;
            }
        }

        return best;
    }

    _Use_decl_annotations_
    StringVoices::Finger* StringVoices::FindFinger(StringState& state, uint32_t touch) noexcept
    {
        for (size_t index = 0; index < state.FingerCount; ++index)
        {
            if (state.Fingers[index].Touch == touch)
            {
                return &state.Fingers[index];
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    void StringVoices::Stop(int32_t string, std::span<StringAction> actions, uint32_t& written) noexcept
    {
        auto& state = m_strings[static_cast<size_t>(string)];

        if (state.Sounding)
        {
            state.Sounding = false;

            if (!OtherStringSounds(string, state.Note))
            {
                Append(actions, written, StringActionKind::NoteOff, string, state.Note, 0.0);
            }
        }

        state.Owner = 0;
        state.Bend = 0.0;
    }

    _Use_decl_annotations_
    void StringVoices::Attack(
        int32_t string,
        int32_t fret,
        double velocity,
        double bend,
        uint32_t owner,
        std::span<StringAction> actions,
        uint32_t& written) noexcept
    {
        Stop(string, actions, written);

        auto const note = FretNote(m_spec, string, fret);

        // A muted fret, or one with no note: the string is damped and plays nothing.
        if (!FretPlays(m_spec, note))
        {
            return;
        }

        auto& state = m_strings[static_cast<size_t>(string)];
        auto const key = static_cast<uint8_t>(note);

        if (!OtherStringSounds(string, key))
        {
            Append(actions, written, StringActionKind::Bend, string, key, bend);
            Append(actions, written, StringActionKind::NoteOn, string, key, HitFrom(velocity));
        }

        state.Sounding = true;
        state.Note = key;
        state.StruckFret = fret;
        state.Owner = owner;
        state.Bend = bend;
    }

    _Use_decl_annotations_
    void StringVoices::Legato(int32_t string, Finger const& finger, std::span<StringAction> actions, uint32_t& written) noexcept
    {
        auto& state = m_strings[static_cast<size_t>(string)];

        if (!state.Sounding)
        {
            Attack(string, finger.Fret, finger.Velocity, finger.Bend, finger.Touch, actions, written);
            return;
        }

        auto const note = FretNote(m_spec, string, finger.Fret);

        // A hammer-on onto a muted fret doesn't sound. The string keeps the note it had.
        if (!FretPlays(m_spec, note))
        {
            return;
        }

        auto const key = static_cast<uint8_t>(note);
        auto const old = state.Note;

        if (key != old)
        {
            state.Note = key;

            if (!OtherStringSounds(string, key))
            {
                Append(actions, written, StringActionKind::Bend, string, key, finger.Bend);
                Append(actions, written, StringActionKind::NoteOn, string, key, HitFrom(finger.Velocity));
            }

            if (!OtherStringSounds(string, old))
            {
                Append(actions, written, StringActionKind::NoteOff, string, old, 0.0);
            }
        }
        else if (std::abs(finger.Bend - state.Bend) >= StringBendStep)
        {
            Append(actions, written, StringActionKind::Bend, string, key, finger.Bend);
        }

        state.StruckFret = finger.Fret;
        state.Owner = finger.Touch;
        state.Bend = finger.Bend;
    }

    _Use_decl_annotations_
    uint32_t StringVoices::Press(
        uint32_t touch,
        int32_t string,
        int32_t fret,
        double fretPosition,
        double velocity,
        std::span<StringAction> actions) noexcept
    {
        uint32_t written{ 0 };

        if (touch == 0 || string < 0 || static_cast<size_t>(string) >= m_spec.Strings.size() ||
            static_cast<size_t>(string) >= m_strings.size() || StringOf(touch) >= 0 || FingerCount() >= MaximumTouches)
        {
            return written;
        }

        auto& state = m_strings[static_cast<size_t>(string)];

        if (state.FingerCount >= MaximumFingersPerString)
        {
            return written;
        }

        auto const others = state.FingerCount > 0;
        auto& finger = state.Fingers[state.FingerCount++];

        finger = Finger{ touch, std::max(fret, 0), fretPosition, 0.0, HitFrom(velocity), ++m_order };

        // Behind a finger already higher on the string, a fret sounds nothing, as on a guitar.
        if (Active(state) != &finger)
        {
            return written;
        }

        if (m_spec.Play == FretPlayMode::Tap)
        {
            if (others && state.Sounding && m_spec.HammerOns)
            {
                Legato(string, finger, actions, written);
            }
            else
            {
                Attack(string, finger.Fret, finger.Velocity, 0.0, touch, actions, written);
            }
        }
        else if (state.Sounding && m_spec.HammerOns)
        {
            // Strumming: fretting is silent, except a hammer-on onto a string that is ringing.
            Legato(string, finger, actions, written);
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t StringVoices::Move(
        uint32_t touch,
        int32_t fret,
        double fretPosition,
        double bend,
        std::span<StringAction> actions) noexcept
    {
        uint32_t written{ 0 };

        auto const string = StringOf(touch);

        if (string < 0)
        {
            return written;
        }

        auto& state = m_strings[static_cast<size_t>(string)];
        auto* const finger = FindFinger(state, touch);

        if (finger == nullptr)
        {
            return written;
        }

        auto const oldFret = finger->Fret;

        finger->Fret = std::max(fret, 0);
        finger->FretPosition = fretPosition;
        finger->Bend = std::isfinite(bend) ? std::max(bend, 0.0) : 0.0;

        auto const* const active = Active(state);

        if (state.Sounding && state.Owner == touch)
        {
            if (m_spec.Slide == FretSlide::Off && finger->Fret != oldFret)
            {
                if (active == finger)
                {
                    Attack(string, finger->Fret, finger->Velocity, finger->Bend, touch, actions, written);
                }
                else if (active != nullptr)
                {
                    // Slid down past another finger, which is now the highest fret held.
                    if (m_spec.HammerOns)
                    {
                        Legato(string, *active, actions, written);
                    }
                    else
                    {
                        Attack(string, active->Fret, active->Velocity, active->Bend, active->Touch, actions, written);
                    }
                }

                return written;
            }

            auto const total = SlideBend(m_spec.Slide, state.StruckFret, finger->Fret, finger->FretPosition) + finger->Bend;

            if (std::abs(total - state.Bend) >= StringBendStep)
            {
                state.Bend = total;
                Append(actions, written, StringActionKind::Bend, string, state.Note, total);
            }

            return written;
        }

        // A finger that moved up past the one sounding takes over, as a hammer-on.
        if (finger->Fret != oldFret && active == finger && state.Owner != touch)
        {
            if (m_spec.Play == FretPlayMode::Tap)
            {
                if (state.Sounding && m_spec.HammerOns)
                {
                    Legato(string, *finger, actions, written);
                }
                else
                {
                    Attack(string, finger->Fret, finger->Velocity, finger->Bend, touch, actions, written);
                }
            }
            else if (state.Sounding && m_spec.HammerOns)
            {
                Legato(string, *finger, actions, written);
            }
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t StringVoices::Release(uint32_t touch, std::span<StringAction> actions) noexcept
    {
        uint32_t written{ 0 };

        auto const string = StringOf(touch);

        if (string < 0)
        {
            return written;
        }

        auto& state = m_strings[static_cast<size_t>(string)];

        for (size_t index = 0; index < state.FingerCount; ++index)
        {
            if (state.Fingers[index].Touch == touch)
            {
                state.Fingers[index] = state.Fingers[state.FingerCount - 1];
                state.Fingers[state.FingerCount - 1] = Finger{};
                state.FingerCount--;
                break;
            }
        }

        if (!state.Sounding || state.Owner != touch)
        {
            return written;
        }

        auto const* const active = Active(state);

        if (active != nullptr)
        {
            // A pull-off: back to the fret still held.
            if (m_spec.HammerOns)
            {
                Legato(string, *active, actions, written);
            }
            else if (m_spec.Play == FretPlayMode::Tap)
            {
                Attack(string, active->Fret, active->Velocity, active->Bend, active->Touch, actions, written);
            }
            else
            {
                state.Owner = 0;
            }

            return written;
        }

        if (m_spec.Sustain == FretSustain::WhileHeld)
        {
            Stop(string, actions, written);
        }
        else
        {
            // Rings on, at the pitch it was left at, until it plays again or is muted.
            state.Owner = 0;
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t StringVoices::Strum(int32_t string, double velocity, std::span<StringAction> actions) noexcept
    {
        uint32_t written{ 0 };

        if (string < 0 || static_cast<size_t>(string) >= m_spec.Strings.size() || static_cast<size_t>(string) >= m_strings.size())
        {
            return written;
        }

        auto const& state = m_strings[static_cast<size_t>(string)];
        auto const* const active = Active(state);

        if (active != nullptr)
        {
            Attack(string, active->Fret, velocity, active->Bend, active->Touch, actions, written);
        }
        else
        {
            Attack(string, 0, velocity, 0.0, 0, actions, written);
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t StringVoices::MuteAll(std::span<StringAction> actions) noexcept
    {
        uint32_t written{ 0 };

        for (size_t index = 0; index < m_strings.size(); ++index)
        {
            Stop(static_cast<int32_t>(index), actions, written);
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t StringVoices::ReleaseAll(std::span<StringAction> actions) noexcept
    {
        for (auto& state : m_strings)
        {
            state.Fingers = {};
            state.FingerCount = 0;
        }

        return MuteAll(actions);
    }

    _Use_decl_annotations_
    bool StringVoices::IsSounding(int32_t string) const noexcept
    {
        return string >= 0 && static_cast<size_t>(string) < m_strings.size() && m_strings[static_cast<size_t>(string)].Sounding;
    }

    _Use_decl_annotations_
    int32_t StringVoices::SoundingNote(int32_t string) const noexcept
    {
        return IsSounding(string) ? m_strings[static_cast<size_t>(string)].Note : -1;
    }

    _Use_decl_annotations_
    int32_t StringVoices::HeldFret(int32_t string) const noexcept
    {
        if (string < 0 || static_cast<size_t>(string) >= m_strings.size())
        {
            return -1;
        }

        auto const* const active = Active(m_strings[static_cast<size_t>(string)]);

        return active == nullptr ? -1 : active->Fret;
    }

    _Use_decl_annotations_
    int32_t StringVoices::StringOf(uint32_t touch) const noexcept
    {
        if (touch == 0)
        {
            return -1;
        }

        for (size_t string = 0; string < m_strings.size(); ++string)
        {
            auto const& state = m_strings[string];

            for (size_t index = 0; index < state.FingerCount; ++index)
            {
                if (state.Fingers[index].Touch == touch)
                {
                    return static_cast<int32_t>(string);
                }
            }
        }

        return -1;
    }

    size_t StringVoices::FingerCount() const noexcept
    {
        size_t count{ 0 };

        for (auto const& state : m_strings)
        {
            count += state.FingerCount;
        }

        return count;
    }

    _Use_decl_annotations_
    uint8_t StringChannelOffset(FretOutput output, int32_t string) noexcept
    {
        auto const index = static_cast<uint8_t>(std::clamp(string, 0, 15));

        switch (output)
        {
        case FretOutput::ChannelPerString:
            return index;

        case FretOutput::Mpe:
            return static_cast<uint8_t>(index + 1);

        default:
            return 0;
        }
    }

    _Use_decl_annotations_
    StringSend StringSendFor(FretOutput output, StringAction const& action) noexcept
    {
        StringSend send{};

        send.ChannelOffset = StringChannelOffset(output, action.String);
        send.Note = action.Note;
        send.Value = action.Value;

        switch (action.Kind)
        {
        case StringActionKind::NoteOn:
            send.Kind = StringSendKind::NoteOn;
            break;

        case StringActionKind::NoteOff:
            send.Kind = StringSendKind::NoteOff;
            break;

        default:
            if (output == FretOutput::PerNoteBend)
            {
                send.Kind = StringSendKind::PerNoteBend;
                send.Sensitivity = FretPerNoteBendSensitivity;
            }
            else
            {
                send.Kind = StringSendKind::ChannelBend;
                send.Sensitivity = output == FretOutput::Mpe ? FretMpeBendSensitivity : FretChannelBendSensitivity;
            }
            break;
        }

        return send;
    }

    _Use_decl_annotations_
    bool StringSetupFor(FretOutput output, int32_t string, StringSend& setup) noexcept
    {
        setup = StringSend{};

        switch (output)
        {
        case FretOutput::PerNoteBend:
            setup.Kind = StringSendKind::PerNoteBendSensitivity;
            setup.Value = FretPerNoteBendSensitivity;
            return true;

        case FretOutput::ChannelPerString:
            setup.Kind = StringSendKind::BendSensitivity;
            setup.ChannelOffset = StringChannelOffset(output, string);
            setup.Value = FretChannelBendSensitivity;
            return true;

        case FretOutput::Mpe:
            // The MPE profile has the sensitivity sent on the manager channel, and never on a
            // member channel.
            setup.Kind = StringSendKind::BendSensitivity;
            setup.ChannelOffset = 0;
            setup.Value = FretMpeBendSensitivity;
            return true;

        default:
            return false;
        }
    }
}
