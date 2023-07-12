// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiUmp32.h"
#include "MidiUmp32.g.cpp"



namespace winrt::Windows::Devices::Midi2::implementation
{
    MidiUmp32::MidiUmp32()
    {
        // use the data buffer as our storage for this. But we have a structure
        // to help provide structure to the data. This minimizes copies
        // and still allows us to use the buffer type with the rest of WinRT
        // and with the service communication.

        uint32_t capacity = sizeof(internal::PackedUmp32);

        Windows::Foundation::IMemoryBufferReference ref = _umpBackingStore.CreateReference();
        WINRT_ASSERT(ref.Capacity() == capacity);

        // get the pointer into the buffer

        auto interop = ref.as<IMemoryBufferByteAccess>();

        uint8_t* bufferData{};
        check_hresult(interop->GetBuffer(&bufferData, &capacity));
        WINRT_ASSERT(capacity == capacity);

        memset(bufferData, 0, capacity);

        // assign the pointer to our UMP structure for ease of access
//        _ump =  reinterpret_cast<internal::PackedUmp32*>(bufferData);
        //_ump = (internal::PackedUmp32*)(bufferData);

      //  WINRT_ASSERT((byte*)_ump == bufferData);


    }

    MidiUmp32::MidiUmp32(internal::MidiTimestamp timestamp, uint32_t word0)
        : MidiUmp32()
    {

        WINRT_ASSERT(_ump != nullptr);

        _timestamp = timestamp;
        _ump->word0 = word0;
    }

    // internal constructor for reading from the service callback
    MidiUmp32::MidiUmp32(internal::MidiTimestamp timestamp, PVOID data)
        : MidiUmp32()
    {
        WINRT_ASSERT(_ump != nullptr);
        WINRT_ASSERT(data != nullptr);

        _timestamp = timestamp;

        // need to have some safeties around this and also make sure it gets deleted
        memcpy((void*)_ump.get(), data, sizeof(internal::PackedUmp32));
    }

    winrt::Windows::Foundation::IMemoryBuffer MidiUmp32::RawData()
    {
        auto sourceRef = _umpBackingStore.CreateReference();
        auto sourceInterop = sourceRef.as<IMemoryBufferByteAccess>();


        auto destination = Windows::Foundation::MemoryBuffer(sizeof(internal::PackedUmp32));
        auto destinationRef = destination.CreateReference();
        auto destinationInterop = destinationRef.as<IMemoryBufferByteAccess>();
        
        destinationInterop.copy_from(sourceInterop.detach());

        return destination;

    }

}
