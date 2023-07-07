// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiMessageReader.g.h"

#include "midi_service_interface.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageReader : MidiMessageReaderT<MidiMessageReader>
    {
        MidiMessageReader() = default;

        MidiMessageReader(hstring const& uniqueId) { _id = uniqueId; }
        hstring Id() { return _id; }

        winrt::Windows::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior EndOfMessagesBehavior();
        void EndOfMessagesBehavior(winrt::Windows::Devices::Midi2::MidiMessageReaderEndOfMessagesBehavior const& value);

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> Filters() { return _filters; }
        winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy FilterStrategy() { return _filterStrategy; }
        void FilterStrategy(winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy const& value) { _filterStrategy = value; }


        bool EndOfMessages();
        uint64_t PeekNextTimestamp();
        winrt::Windows::Devices::Midi2::MidiUmpSize PeekNextUmpSize();
        winrt::Windows::Devices::Midi2::MidiUmp32 ReadUmp32();
        winrt::Windows::Devices::Midi2::MidiUmp64 ReadUmp64();
        winrt::Windows::Devices::Midi2::MidiUmp96 ReadUmp96();
        winrt::Windows::Devices::Midi2::MidiUmp128 ReadUmp128();
        uint32_t ReadBuffer(winrt::Windows::Foundation::IMemoryBufferReference const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToRead);


        // impl methods that do not show up in the runtimeclass
        // these methods are called by the endpoint when deciding if the messages received event should be fired
        bool ReceiveUmp32(winrt::Windows::Devices::Midi2::MidiUmp32 ump);
        bool ReceiveUmp64(winrt::Windows::Devices::Midi2::MidiUmp64 ump);
        bool ReceiveUmp96(winrt::Windows::Devices::Midi2::MidiUmp96 ump);
        bool ReceiveUmp128(winrt::Windows::Devices::Midi2::MidiUmp128 ump);



    private:
        hstring _id{};

        MidiMessageClientFilterStrategy _filterStrategy{ MidiMessageClientFilterStrategy::OrFilters };
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> _filters{ winrt::single_threaded_vector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter>() };

            


    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageReader : MidiMessageReaderT<MidiMessageReader, implementation::MidiMessageReader>
    {
    };
}