#include "pch.h"
#include "Midi2RegisteredPerNoteControllerMessage.h"
#include "Midi2RegisteredPerNoteControllerMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2RegisteredPerNoteControllerMessage::NoteNumber()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2RegisteredPerNoteControllerMessage::Index()
    {
        throw hresult_not_implemented();
    }
    uint32_t Midi2RegisteredPerNoteControllerMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
