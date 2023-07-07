// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiBidirectionalEndpointConnection.h"
#include "MidiBidirectionalEndpointConnection.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    hstring MidiBidirectionalEndpointConnection::GetDeviceSelectorForBidirectional()
    {
        // TODO
        return L"";
    }

    IFACEMETHODIMP MidiBidirectionalEndpointConnection::Callback(PVOID Data, UINT Size, LONGLONG Position)
    {
        // TODO: process incoming messages / fire event

        return S_OK;
    }

    // internal method to start listening for incoming messages, enable processing outgoing messages, etc.
    void MidiBidirectionalEndpointConnection::Start()
    {
        // assuming bi-directional MIDI connection right now
        winrt::com_ptr<IMidiBiDi> umpEndpoint;

        // We're talking to the service, so use the MIDI Service abstraction, not a KS or other one
        auto abstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

        // get our BiDi endpoint
        winrt::check_hresult(abstraction->Activate(__uuidof(IMidiBiDi), (void**)&umpEndpoint));

        // start up the endpoint
        if (_useMmcss)
        {
            winrt::check_hresult(umpEndpoint->Initialize((LPCWSTR)(_deviceId.c_str()), &_mmcssTaskId, (IMidiCallback*)this));
        }
        else
        {
            // TODO: Need another call or parameter set for the abstraction to tell it not to use mmcss
        }




        // todo: wire up the reader/writer to be able to call SendMidiMessage() etc.

      //  _messageReader = foo;
      //  _messageWriter = foo;
    }


    winrt::event_token MidiBidirectionalEndpointConnection::MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler)
    {
        throw hresult_not_implemented();
    }
    void MidiBidirectionalEndpointConnection::MessagesReceived(winrt::event_token const& token) noexcept
    {
        throw hresult_not_implemented();
    }
}
