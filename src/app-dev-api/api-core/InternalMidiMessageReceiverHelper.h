#pragma once

#include "MidiMessageReceivedEventArgs.h"

namespace Windows::Devices::Midi2::Internal
{
    class InternalMidiMessageReceiverHelper
    {
    public:
        winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs CreateEventArgsFromCallbackParams(PVOID Data, UINT Size, LONGLONG Position);


    private:

    };
}
