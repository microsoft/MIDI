#include "pch.h"
#include "Midi2PerNoteManagementMessage.h"
#include "Midi2PerNoteManagementMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2PerNoteManagementMessage::NoteNumber()
    {
        throw hresult_not_implemented();
    }
    bool Midi2PerNoteManagementMessage::DetachPerNoteControllers()
    {
        throw hresult_not_implemented();
    }
    bool Midi2PerNoteManagementMessage::ResetPerNoteControllers()
    {
        throw hresult_not_implemented();
    }
}
