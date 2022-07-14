#pragma once
#include "Midi2AssignablePerNoteControllerMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2AssignablePerNoteControllerMessage : Midi2AssignablePerNoteControllerMessageT<Midi2AssignablePerNoteControllerMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2AssignablePerNoteControllerMessage() = default;

        uint8_t NoteNumber();
        uint8_t Index();
        uint32_t Data();
    };
}
