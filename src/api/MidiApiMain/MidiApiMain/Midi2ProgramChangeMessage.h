#pragma once
#include "Midi2ProgramChangeMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2ProgramChangeMessage : Midi2ProgramChangeMessageT<Midi2ProgramChangeMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2ProgramChangeMessage() = default;

        bool BankValid();
        uint8_t Program();
        uint8_t BankMsb();
        uint8_t BankLsb();
        uint16_t CombinedBank();
    };
}
