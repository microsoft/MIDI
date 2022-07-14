#pragma once
#include "Midi2RelativeRegisteredControllerMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2RelativeRegisteredControllerMessage : Midi2RelativeRegisteredControllerMessageT<Midi2RelativeRegisteredControllerMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2RelativeRegisteredControllerMessage() = default;

        uint8_t Bank();
        uint8_t Index();
        uint32_t Data();
    };
}
