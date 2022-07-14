#include "pch.h"
#include "Midi2ChannelVoiceMessage.h"
#include "Midi2ChannelVoiceMessage.g.cpp"

namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2ChannelVoiceMessage::Channel()
    {
        throw hresult_not_implemented();
    }
}
