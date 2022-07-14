#include "pch.h"
#include "Midi1ProgramChangeMessage.h"
#include "Midi1ProgramChangeMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi1ProgramChangeMessage::Program()
    {
        throw hresult_not_implemented();
    }
}
