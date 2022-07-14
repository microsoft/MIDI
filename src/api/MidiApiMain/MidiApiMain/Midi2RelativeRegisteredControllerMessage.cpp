#include "pch.h"
#include "Midi2RelativeRegisteredControllerMessage.h"
#include "Midi2RelativeRegisteredControllerMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2RelativeRegisteredControllerMessage::Bank()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2RelativeRegisteredControllerMessage::Index()
    {
        throw hresult_not_implemented();
    }
    uint32_t Midi2RelativeRegisteredControllerMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
