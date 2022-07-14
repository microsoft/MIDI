#pragma once
#include "Midi2RegisteredPerNoteControllerMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2RegisteredPerNoteControllerMessage : Midi2RegisteredPerNoteControllerMessageT<Midi2RegisteredPerNoteControllerMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2RegisteredPerNoteControllerMessage() = default;

        uint8_t NoteNumber();
        uint8_t Index();
        uint32_t Data();
    };
}
