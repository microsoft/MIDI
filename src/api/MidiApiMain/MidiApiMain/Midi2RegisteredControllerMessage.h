#pragma once
#include "Midi2RegisteredControllerMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2RegisteredControllerMessage : Midi2RegisteredControllerMessageT<Midi2RegisteredControllerMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2RegisteredControllerMessage() = default;

        uint8_t Bank();
        uint8_t Index();
        uint32_t Data();
    };
}
