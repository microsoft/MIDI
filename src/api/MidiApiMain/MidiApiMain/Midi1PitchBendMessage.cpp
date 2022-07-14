#include "pch.h"
#include "Midi1PitchBendMessage.h"
#include "Midi1PitchBendMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi1PitchBendMessage::DataLsb()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi1PitchBendMessage::DataMsb()
    {
        throw hresult_not_implemented();
    }
    uint16_t Midi1PitchBendMessage::CombinedBendData()
    {
        throw hresult_not_implemented();
    }
}
