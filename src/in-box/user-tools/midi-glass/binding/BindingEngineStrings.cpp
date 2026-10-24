// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// What a fretboard puts on the wire. Deliberately free of pch.h, XAML and the MIDI SDK, like the
// rest of the binding engine.

#include "BindingEngine.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        constexpr uint8_t StatusPitchBend = 0xE;
        constexpr uint8_t StatusPerNotePitchBend = 0x6;
        constexpr uint8_t StatusRegisteredController = 0x2;

        // Registered controller bank 0: index 0 is pitch bend sensitivity, and index 7 is the
        // sensitivity of per-note pitch bend.
        constexpr uint8_t BendSensitivityIndex = 0;
        constexpr uint8_t PerNoteBendSensitivityIndex = 7;

        // A 14 bit MIDI 1.0 pitch bend, centered on 8192.
        uint16_t Midi1BendValue(_In_ double semitones, _In_ double sensitivity) noexcept
        {
            if (!std::isfinite(semitones) || !std::isfinite(sensitivity) || sensitivity <= 0.0)
            {
                return 8192;
            }

            auto const fraction = std::clamp(semitones / sensitivity, -1.0, 1.0);

            return static_cast<uint16_t>(std::clamp<long long>(std::llround(8192.0 + fraction * 8192.0), 0, 16383));
        }
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluateString(
        size_t controlIndex,
        StringSend const& request,
        std::span<PreparedSend> sends) const noexcept
    {
        if (controlIndex >= m_controls.size() || sends.empty())
        {
            return 0;
        }

        auto const& control = m_controls[controlIndex];

        uint32_t written{ 0 };

        for (uint32_t i = 0; i < control.MessageCount && written < sends.size(); ++i)
        {
            auto entry = m_messages[control.FirstMessage + i];

            if (entry.Kind != MessageKind::Note)
            {
                continue;
            }

            if (entry.DestinationIndex < 0 ||
                static_cast<size_t>(entry.DestinationIndex) >= m_destinations.size() ||
                !m_destinations[static_cast<size_t>(entry.DestinationIndex)].IsAvailable)
            {
                continue;
            }

            auto const channel = static_cast<uint8_t>((entry.ChannelIndex + request.ChannelOffset) & 0x0F);
            auto const note = static_cast<uint8_t>(request.Note & 0x7F);
            auto& send = sends[written];

            send.WordCount = 0;

            switch (request.Kind)
            {
            case StringSendKind::NoteOn:
            case StringSendKind::NoteOff:
                entry.Number = note;
                entry.ChannelIndex = channel;
                send.WordCount = BuildNoteWords(entry, request.Kind == StringSendKind::NoteOn, request.Value, send.Words);
                break;

            case StringSendKind::ChannelBend:
                if (entry.UseMidi1Protocol)
                {
                    auto const bend = Midi1BendValue(request.Value, request.Sensitivity);

                    send.Words[0] = BuildMidi1ChannelVoice(
                        entry.GroupIndex, StatusPitchBend, channel,
                        static_cast<uint8_t>(bend & 0x7F), static_cast<uint8_t>((bend >> 7) & 0x7F));
                    send.WordCount = 1;
                }
                else
                {
                    BuildMidi2ChannelVoice(
                        entry.GroupIndex, StatusPitchBend, channel, 0, 0,
                        PerNotePitchBendValue(request.Value, request.Sensitivity), send.Words);
                    send.WordCount = 2;
                }
                break;

            case StringSendKind::PerNoteBend:
                if (!entry.UseMidi1Protocol)
                {
                    BuildMidi2ChannelVoice(
                        entry.GroupIndex, StatusPerNotePitchBend, channel, note, 0,
                        PerNotePitchBendValue(request.Value, request.Sensitivity), send.Words);
                    send.WordCount = 2;
                }
                break;

            case StringSendKind::BendSensitivity:
                BuildMidi2ChannelVoice(
                    entry.GroupIndex, StatusRegisteredController, channel, 0, BendSensitivityIndex,
                    SemitonesAsPitch725(request.Value), send.Words);
                send.WordCount = 2;
                break;

            case StringSendKind::PerNoteBendSensitivity:
                if (!entry.UseMidi1Protocol)
                {
                    BuildMidi2ChannelVoice(
                        entry.GroupIndex, StatusRegisteredController, channel, 0, PerNoteBendSensitivityIndex,
                        SemitonesAsPitch725(request.Value), send.Words);
                    send.WordCount = 2;
                }
                break;

            default:
                break;
            }

            if (send.WordCount == 0)
            {
                continue;
            }

            send.DestinationIndex = entry.DestinationIndex;
            ++written;
        }

        return written;
    }
}
