#pragma once
#include "Midi2RelativeAssignableControllerMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2RelativeAssignableControllerMessage : Midi2RelativeAssignableControllerMessageT<Midi2RelativeAssignableControllerMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2RelativeAssignableControllerMessage() = default;

        uint8_t Bank();
        uint8_t Index();
        uint32_t Data();
    };
}
