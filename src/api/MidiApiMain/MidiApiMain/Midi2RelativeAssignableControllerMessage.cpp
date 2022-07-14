#include "pch.h"
#include "Midi2RelativeAssignableControllerMessage.h"
#include "Midi2RelativeAssignableControllerMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2RelativeAssignableControllerMessage::Bank()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2RelativeAssignableControllerMessage::Index()
    {
        throw hresult_not_implemented();
    }
    uint32_t Midi2RelativeAssignableControllerMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
