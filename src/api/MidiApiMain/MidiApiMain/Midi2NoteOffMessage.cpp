#include "pch.h"
#include "Midi2NoteOffMessage.h"
#include "Midi2NoteOffMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2NoteOffMessage::NoteNumber()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2NoteOffMessage::AttributeType()
    {
        throw hresult_not_implemented();
    }
    uint16_t Midi2NoteOffMessage::Velocity()
    {
        throw hresult_not_implemented();
    }
    uint16_t Midi2NoteOffMessage::AttributeData()
    {
        throw hresult_not_implemented();
    }
}
