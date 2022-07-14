#include "pch.h"
#include "Midi2RegisteredControllerMessage.h"
#include "Midi2RegisteredControllerMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2RegisteredControllerMessage::Bank()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2RegisteredControllerMessage::Index()
    {
        throw hresult_not_implemented();
    }
    uint32_t Midi2RegisteredControllerMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
