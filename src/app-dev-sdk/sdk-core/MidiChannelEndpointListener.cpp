// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiChannelEndpointListener.h"
#include "MidiChannelEndpointListener.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiGroup MidiChannelEndpointListener::IncludeGroup()
    {
        throw hresult_not_implemented();
    }
    void MidiChannelEndpointListener::IncludeGroup(winrt::Microsoft::Devices::Midi2::MidiGroup const& value)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiChannel> MidiChannelEndpointListener::IncludeChannels()
    {
        throw hresult_not_implemented();
    }

    void MidiChannelEndpointListener::ProcessIncomingMessage(winrt::Windows::Devices::Midi2::IMidiInputConnection const& sourceConnection, winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args, bool& stopProcessing, bool& removeMessageFromQueue)
    {
        throw hresult_not_implemented();
    }
}
