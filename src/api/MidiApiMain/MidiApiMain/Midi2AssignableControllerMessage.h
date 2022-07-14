#pragma once
#include "Midi2AssignableControllerMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2AssignableControllerMessage : Midi2AssignableControllerMessageT<Midi2AssignableControllerMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2AssignableControllerMessage() = default;

        uint8_t Bank();
        uint8_t Index();
        uint32_t Data();
    };
}
