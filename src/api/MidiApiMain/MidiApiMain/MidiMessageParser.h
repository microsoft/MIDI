#pragma once
#include "MidiMessageParser.g.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiMessageParser
    {
        MidiMessageParser() = default;

        static winrt::Windows::Foundation::Collections::IVector<bool> GetTypedMessagesFromStream(winrt::Windows::Devices::Midi::MidiStream const& stream);
    };
}
namespace winrt::Windows::Devices::Midi::factory_implementation
{
    struct MidiMessageParser : MidiMessageParserT<MidiMessageParser, implementation::MidiMessageParser>
    {
    };
}
