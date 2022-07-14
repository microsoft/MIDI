#include "pch.h"
#include "Midi1ControlChangeMessage.h"
#include "Midi1ControlChangeMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi1ControlChangeMessage::Index()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi1ControlChangeMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
