#include "pch.h"
#include "Midi2PolyPressureMessage.h"
#include "Midi2PolyPressureMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint8_t Midi2PolyPressureMessage::NoteNumber()
    {
        throw hresult_not_implemented();
    }
    uint32_t Midi2PolyPressureMessage::Data()
    {
        throw hresult_not_implemented();
    }
}
