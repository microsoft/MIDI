// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiCiProxy.h"
#include "MidiCiProxy.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    void MidiCiProxy::Initialize()
    {
        throw hresult_not_implemented();
    }

    void MidiCiProxy::ProcessIncomingMessage(
        _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& /*args*/,
        _Out_ bool& skipFurtherListeners,
        _Out_ bool& skipMainMessageReceivedEvent)
    {
        skipFurtherListeners = false;
        skipMainMessageReceivedEvent = false;

        throw hresult_not_implemented();
    }

    winrt::Windows::Foundation::IAsyncAction MidiCiProxy::ProcessIncomingMessageAsync(
        _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs /*args*/)
    {
        throw hresult_not_implemented();
    }

    void MidiCiProxy::Cleanup()
    {
        throw hresult_not_implemented();
    }
}
