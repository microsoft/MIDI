#include "pch.h"
#include "Midi1PolyPressureMessage.h"
#include "Midi1PolyPressureMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi1PolyPressureMessage::NoteNumber()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi1PolyPressureMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
