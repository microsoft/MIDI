#include "pch.h"
#include "InternalMidiMessageReceiverHelper.h"

#include "MidiUmp32.h"
#include "MidiUmp64.h"
#include "MidiUmp96.h"
#include "MidiUmp128.h"

namespace implementation = winrt::Windows::Devices::Midi2::implementation;

namespace Windows::Devices::Midi2::Internal
{

	winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs InternalMidiMessageReceiverHelper::CreateEventArgsFromCallbackParams(PVOID Data, UINT Size, LONGLONG Timestamp)
	{
        auto args = winrt::make_self<implementation::MidiMessageReceivedEventArgs>();

        // should probably factor this out into a UMP Builder class

        if (Size == sizeof(internal::PackedUmp32))
        {
            args->Ump(winrt::make_self<implementation::MidiUmp32>(Timestamp, Data).as<winrt::Windows::Devices::Midi2::IMidiUmp>());
        }
        else if (Size == sizeof(internal::PackedUmp64))
        {
            args->Ump(winrt::make_self<implementation::MidiUmp64>(Timestamp, Data).as<winrt::Windows::Devices::Midi2::IMidiUmp>());
        }
        else if (Size == sizeof(internal::PackedUmp96))
        {
            args->Ump(winrt::make_self<implementation::MidiUmp96>(Timestamp, Data).as<winrt::Windows::Devices::Midi2::IMidiUmp>());
        }
        else if (Size == sizeof(internal::PackedUmp128))
        {
            args->Ump(winrt::make_self<implementation::MidiUmp128>(Timestamp, Data).as<winrt::Windows::Devices::Midi2::IMidiUmp>());
        }
        else
        {
            // shouldn't happen until we support more than one message
            return nullptr;
        }

        return *args;

	}


}