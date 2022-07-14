#include "pch.h"
#include "Midi1NoteOnMessage.h"
#include "Midi1NoteOnMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi1NoteOnMessage::NoteNumber()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi1NoteOnMessage::Velocity()
    {
        throw hresult_not_implemented();
    }
}
