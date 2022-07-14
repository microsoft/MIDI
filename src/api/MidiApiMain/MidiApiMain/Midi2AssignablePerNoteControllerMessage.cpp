#include "pch.h"
#include "Midi2AssignablePerNoteControllerMessage.h"
#include "Midi2AssignablePerNoteControllerMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2AssignablePerNoteControllerMessage::NoteNumber()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2AssignablePerNoteControllerMessage::Index()
    {
        throw hresult_not_implemented();
    }
    uint32_t Midi2AssignablePerNoteControllerMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
