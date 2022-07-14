#include "pch.h"
#include "Midi2ControlChangeMessage.h"
#include "Midi2ControlChangeMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2ControlChangeMessage::Index()
    {
        throw hresult_not_implemented();
    }
    uint32_t Midi2ControlChangeMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
