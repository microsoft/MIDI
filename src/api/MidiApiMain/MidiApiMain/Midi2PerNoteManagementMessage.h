#pragma once
#include "Midi2PerNoteManagementMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2PerNoteManagementMessage : Midi2PerNoteManagementMessageT<Midi2PerNoteManagementMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2PerNoteManagementMessage() = default;

        uint8_t NoteNumber();
        bool DetachPerNoteControllers();
        bool ResetPerNoteControllers();
    };
}
