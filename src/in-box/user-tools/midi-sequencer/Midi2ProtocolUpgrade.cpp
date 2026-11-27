// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "Midi2ProtocolUpgrade.h"

#include <algorithm>
#include <array>
#include <optional>

#pragma warning(push, 0)
#pragma warning(disable: 4996)
#include <libmidi2/umpToMIDI2Protocol.h>
#pragma warning(pop)

namespace midisequencer
{
    namespace
    {
        constexpr uint32_t ControllerDataEntryMsb = 6;
        constexpr uint32_t ControllerDataEntryLsb = 38;
        constexpr uint32_t ControllerNrpnLsb = 98;
        constexpr uint32_t ControllerNrpnMsb = 99;
        constexpr uint32_t ControllerRpnLsb = 100;
        constexpr uint32_t ControllerRpnMsb = 101;

        // Group and channel, as libmidi2 keeps its state: group in the high four bits.
        size_t GroupChannel(_In_ uint32_t word) noexcept
        {
            return static_cast<size_t>((((word >> 24) & 0x0F) << 4) | ((word >> 16) & 0x0F));
        }
    }

    // What libmidi2 holds for each group and channel, kept here too so a held data entry can be
    // sent when this code decides, and with the tick it came at.
    struct Midi2ProtocolUpgrade::State
    {
        struct Selection
        {
            uint8_t Msb{ 255 };
            uint8_t Lsb{ 255 };
            bool Registered{ true };
            bool Held{ false };
            int64_t HeldTick{ 0 };
        };

        umpToMIDI2Protocol Translator{};
        std::array<Selection, 256> Selections{};
    };

    Midi2ProtocolUpgrade::Midi2ProtocolUpgrade() :
        m_state{ std::make_unique<State>() }
    {
    }

    Midi2ProtocolUpgrade::~Midi2ProtocolUpgrade() = default;

    _Use_decl_annotations_
    void Midi2ProtocolUpgrade::Upgrade(std::vector<ClipEvent>& events)
    {
        std::stable_sort(events.begin(), events.end(), [](ClipEvent const& a, ClipEvent const& b) { return a.Tick < b.Tick; });

        auto& translator = m_state->Translator;
        auto& selections = m_state->Selections;

        std::vector<ClipEvent> upgraded{};
        upgraded.reserve(events.size());

        // Feeds one MIDI 1.0 word and keeps what comes out. A registered or assignable controller
        // that was held goes out at the tick of its data entry, unless its CC 38 completed it.
        auto feed = [&](uint32_t word, int64_t tick, State::Selection const& selection, bool completesHeld)
        {
            auto const heldTick = selection.Held && !completesHeld ? std::optional<int64_t>{ selection.HeldTick } : std::nullopt;

            translator.UMPStreamParse(word);

            while (translator.availableUMP())
            {
                ClipEvent out{};
                out.Words[0] = translator.readUMP();
                out.WordCount = 1;

                auto const midi2 = (out.Words[0] >> 28) == 0x4;

                if (midi2 && translator.availableUMP())
                {
                    out.Words[1] = translator.readUMP();
                    out.WordCount = 2;
                }

                auto const status = (out.Words[0] >> 20) & 0x0F;
                auto const controller = midi2 && (status == 0x2 || status == 0x3);

                out.Tick = controller && heldTick.has_value() ? *heldTick : tick;
                upgraded.push_back(out);
            }
        };

        // Selecting the same parameter again sends what's held and changes nothing else. libmidi2
        // has no call for this yet (AM_MIDI2.0Lib issue #47).
        auto flush = [&](size_t groupChannel)
        {
            auto& selection = selections[groupChannel];

            if (!selection.Held)
            {
                return;
            }

            auto const word = 0x20B00000u |
                (static_cast<uint32_t>(groupChannel >> 4) << 24) |
                (static_cast<uint32_t>(groupChannel & 0x0F) << 16) |
                ((selection.Registered ? ControllerRpnMsb : ControllerNrpnMsb) << 8) |
                selection.Msb;

            feed(word, selection.HeldTick, selection, false);
            selection.Held = false;
        };

        for (auto const& event : events)
        {
            auto const word = event.Words[0];
            auto const messageType = word >> 28;

            if (event.WordCount == 0 || (messageType != 0x2 && messageType != 0x4))
            {
                upgraded.push_back(event);
                continue;
            }

            auto const groupChannel = GroupChannel(word);
            auto& selection = selections[groupChannel];

            if (messageType == 0x4)
            {
                flush(groupChannel);
                upgraded.push_back(event);
                continue;
            }

            if (((word >> 20) & 0x0F) != 0xB)
            {
                flush(groupChannel);
                feed(word, event.Tick, selection, false);
                continue;
            }

            auto const controller = (word >> 8) & 0x7F;
            auto const value = static_cast<uint8_t>(word & 0x7F);

            feed(word, event.Tick, selection, controller == ControllerDataEntryLsb);

            switch (controller)
            {
            case ControllerRpnMsb:
            case ControllerNrpnMsb:
                selection.Registered = controller == ControllerRpnMsb;
                selection.Msb = value;
                selection.Held = false;
                break;

            case ControllerRpnLsb:
            case ControllerNrpnLsb:
                selection.Registered = controller == ControllerRpnLsb;
                selection.Lsb = value;
                selection.Held = false;
                break;

            case ControllerDataEntryMsb:
            {
                // libmidi2 sends RPN 0x0002, 0x0003, 0x0004 and 0x0006 at once, without waiting
                // for CC 38, because they use only the MSB. It never sends the null function.
                auto const sentAtOnce = selection.Registered && selection.Msb == 0 &&
                    selection.Lsb >= 2 && selection.Lsb <= 6 && selection.Lsb != 5;
                auto const null = selection.Msb == 0x7F && selection.Lsb == 0x7F;
                selection.Held = !sentAtOnce && !null && selection.Msb < 128 && selection.Lsb < 128;
                selection.HeldTick = event.Tick;
                break;
            }

            default:
                // CC 38, or any other controller, sends what was held.
                selection.Held = false;
                break;
            }
        }

        for (size_t groupChannel = 0; groupChannel < selections.size(); ++groupChannel)
        {
            flush(groupChannel);
        }

        std::stable_sort(upgraded.begin(), upgraded.end(), [](ClipEvent const& a, ClipEvent const& b) { return a.Tick < b.Tick; });
        events = std::move(upgraded);
    }
}
