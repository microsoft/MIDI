// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageBuffer.h"
#include "MidiMessageBuffer.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    //MidiMessageBuffer::MidiMessageBuffer(uint32_t sizeInBytes)
    //{
    //    throw hresult_not_implemented();
    //}
    void MidiMessageBuffer::Close()
    {
        throw hresult_not_implemented();


    }
    winrt::Windows::Foundation::IMemoryBufferReference MidiMessageBuffer::CreateReference()
    {
        throw hresult_not_implemented();
    }
}
