#pragma once
#include "MidiMessagesReceivedEventArgs.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiMessagesReceivedEventArgs : MidiMessagesReceivedEventArgsT<MidiMessagesReceivedEventArgs>
    {
        MidiMessagesReceivedEventArgs() = default;

        hstring SourceMidiEndpointId();
        winrt::Microsoft::Devices::Midi2::MidiMessageReader GetMessageReader();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiMessagesReceivedEventArgs : MidiMessagesReceivedEventArgsT<MidiMessagesReceivedEventArgs, implementation::MidiMessagesReceivedEventArgs>
    {
    };
}
