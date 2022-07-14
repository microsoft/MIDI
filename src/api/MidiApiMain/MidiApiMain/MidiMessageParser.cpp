#include "pch.h"
#include "MidiMessageParser.h"
#include "MidiMessageParser.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    winrt::Windows::Foundation::Collections::IVector<bool> MidiMessageParser::GetTypedMessagesFromStream(winrt::Windows::Devices::Midi::MidiStream const& stream)
    {
        throw hresult_not_implemented();
    }
}
