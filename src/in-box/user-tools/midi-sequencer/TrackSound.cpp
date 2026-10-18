// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "TrackSound.h"

#include <algorithm>

namespace midisequencer
{
    namespace
    {
        constexpr uint8_t BankSelectMsb = 0;
        constexpr uint8_t BankSelectLsb = 32;

        uint32_t MessageType(ClipEvent const& message) noexcept
        {
            return message.WordCount > 0 ? message.Words[0] >> 28 : 0;
        }

        uint32_t Status(ClipEvent const& message) noexcept
        {
            return (message.Words[0] >> 20) & 0x0F;
        }

        bool IsChannelVoice(ClipEvent const& message) noexcept
        {
            auto const type = MessageType(message);
            return (type == 0x2 && message.WordCount >= 1) || (type == 0x4 && message.WordCount >= 2);
        }

        // A controller's value as MIDI 1.0 has it. A MIDI 2.0 value keeps its top 7 bits, which
        // is what scaling it down gives.
        uint8_t ControllerValue(ClipEvent const& message) noexcept
        {
            return MessageType(message) == 0x2
                ? static_cast<uint8_t>(message.Words[0] & 0x7F)
                : static_cast<uint8_t>(message.Words[1] >> 25);
        }

        uint8_t ControllerNumber(ClipEvent const& message) noexcept
        {
            return static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F);
        }
    }

    _Use_decl_annotations_
    bool IsSoundMessage(ClipEvent const& message) noexcept
    {
        if (!IsChannelVoice(message))
        {
            return false;
        }

        auto const status = Status(message);

        if (status == 0xC)
        {
            return true;
        }

        auto const controller = ControllerNumber(message);
        return status == 0xB && (controller == BankSelectMsb || controller == BankSelectLsb);
    }

    _Use_decl_annotations_
    TrackSound StartupSound(std::vector<ClipEvent> const& startup) noexcept
    {
        TrackSound sound{};

        // The bank select controllers apply to the program change after them.
        bool seenMsb{ false };
        bool seenLsb{ false };
        uint8_t msb{ 0 };
        uint8_t lsb{ 0 };

        for (auto const& message : startup)
        {
            if (!IsChannelVoice(message))
            {
                continue;
            }

            auto const status = Status(message);

            if (status == 0xB)
            {
                auto const controller = ControllerNumber(message);

                if (controller == BankSelectMsb)
                {
                    msb = ControllerValue(message);
                    seenMsb = true;
                }
                else if (controller == BankSelectLsb)
                {
                    lsb = ControllerValue(message);
                    seenLsb = true;
                }

                continue;
            }

            if (status != 0xC)
            {
                continue;
            }

            sound = TrackSound{};
            sound.Set = true;

            if (MessageType(message) == 0x2)
            {
                sound.Program = static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F);
            }
            else
            {
                sound.Program = static_cast<uint8_t>((message.Words[1] >> 24) & 0x7F);

                // Its own bank wins over the controllers.
                if ((message.Words[0] & 0x01) != 0)
                {
                    sound.HasBank = true;
                    sound.BankMsb = static_cast<uint8_t>((message.Words[1] >> 8) & 0x7F);
                    sound.BankLsb = static_cast<uint8_t>(message.Words[1] & 0x7F);
                    continue;
                }
            }

            if (seenMsb || seenLsb)
            {
                sound.HasBank = true;
                sound.BankMsb = msb;
                sound.BankLsb = lsb;
            }
        }

        return sound;
    }

    _Use_decl_annotations_
    ClipEvent SoundMessage(TrackSound const& sound, uint8_t channel) noexcept
    {
        ClipEvent message{};
        message.Words[0] = 0x40000000u | (0xCu << 20) | (static_cast<uint32_t>(channel & 0x0F) << 16) | (sound.HasBank ? 0x01u : 0x00u);
        message.Words[1] = (static_cast<uint32_t>(sound.Program & 0x7F) << 24);

        if (sound.HasBank)
        {
            message.Words[1] |= (static_cast<uint32_t>(sound.BankMsb & 0x7F) << 8) | static_cast<uint32_t>(sound.BankLsb & 0x7F);
        }

        message.WordCount = 2;
        return message;
    }

    _Use_decl_annotations_
    void SetStartupSound(std::vector<ClipEvent>& startup, TrackSound const& sound, uint8_t channel)
    {
        std::erase_if(startup, [](ClipEvent const& message) { return IsSoundMessage(message); });

        if (sound.Set)
        {
            startup.insert(startup.begin(), SoundMessage(sound, channel));
        }
    }
}
