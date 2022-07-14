#include "pch.h"
#include "Midi2AssignableControllerMessage.h"
#include "Midi2AssignableControllerMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2AssignableControllerMessage::Bank()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2AssignableControllerMessage::Index()
    {
        throw hresult_not_implemented();
    }
    uint32_t Midi2AssignableControllerMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
