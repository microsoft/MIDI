// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointInformation.h"
#include "MidiEndpointInformation.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiEndpointInformation::Name()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiEndpointInformation::UmpVersionMajor()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiEndpointInformation::UmpVersionMinor()
    {
        throw hresult_not_implemented();
    }
    bool MidiEndpointInformation::HasStaticFunctionBlocks()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiEndpointInformation::FunctionBlockCount()
    {
        throw hresult_not_implemented();
    }
    hstring MidiEndpointInformation::ProductInstanceId()
    {
        throw hresult_not_implemented();
    }
    bool MidiEndpointInformation::SupportsMidi10Protocol()
    {
        throw hresult_not_implemented();
    }
    bool MidiEndpointInformation::SupportsMidi20Protocol()
    {
        throw hresult_not_implemented();
    }
    bool MidiEndpointInformation::SupportsReceivingJRTimestamps()
    {
        throw hresult_not_implemented();
    }
    bool MidiEndpointInformation::SupportsSendingJRTimestamps()
    {
        throw hresult_not_implemented();
    }
}
