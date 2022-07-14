#include "pch.h"
#include "Midi1NoteOffMessage.h"
#include "Midi1NoteOffMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi1NoteOffMessage::NoteNumber()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi1NoteOffMessage::Velocity()
    {
        throw hresult_not_implemented();
    }
}
