// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services Client SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiEndpoint.h"
#include "MidiEndpoint.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiEndpoint::GetDeviceSelector()
    {
        throw hresult_not_implemented();
    }
    hstring MidiEndpoint::DeviceId()
    {
        throw hresult_not_implemented();
    }
    bool MidiEndpoint::EndpointInformationValid()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiEndpointInformation MidiEndpoint::EndpointInformation()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiEndpointType MidiEndpoint::EndpointType()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Devices::Midi2::MidiFunctionBlock> MidiEndpoint::FunctionBlocks()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiGroupTerminalBlock> MidiEndpoint::GroupTerminalBlocks()
    {
        throw hresult_not_implemented();
    }
    void MidiEndpoint::SendUmp(uint64_t timestamp, winrt::Microsoft::Devices::Midi2::Ump const& ump)
    {
        throw hresult_not_implemented();
    }
    void MidiEndpoint::SendMultipleUmps(uint64_t timestamp, winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::Ump> const& umps)
    {
        throw hresult_not_implemented();
    }
    void MidiEndpoint::SendMultipleUmpsAsWords(uint64_t timestamp, winrt::Windows::Foundation::Collections::IVector<uint32_t> const& umpWords)
    {
        throw hresult_not_implemented();
    }
}
