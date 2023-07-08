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

// TEMP
#include <iostream>

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
    bool MidiBidirectionalEndpointConnection::Start()
    {
        // assuming bi-directional MIDI connection right now
        winrt::com_ptr<IMidiBiDi> umpEndpoint;

        std::cout << "MidiBidirectionalEndpointConnection::Start: DeviceId=" << winrt::to_string(_deviceId) << std::endl;

        std::cout << "MidiBidirectionalEndpointConnection::Start: About to create MidiSrvAbstraction" << std::endl;

        // We're talking to the service, so use the MIDI Service abstraction, not a KS or other one
        auto abstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

        std::cout << "MidiBidirectionalEndpointConnection::Start: About to activate IMidiBiDi" << std::endl;

        // get our BiDi endpoint
        winrt::check_hresult(abstraction->Activate(__uuidof(IMidiBiDi), (void**)&umpEndpoint));

        // start up the endpoint
        std::cout << "MidiBidirectionalEndpointConnection::Start: About to initialize." << std::endl;

        try
        {
            if (_useMmcss)
            {
                winrt::check_hresult(umpEndpoint->Initialize((LPCWSTR)(_deviceId.c_str()), &_mmcssTaskId, (IMidiCallback*)this));
            }
            else
            {
                // TODO: Need another call or parameter set for the abstraction to tell it not to use mmcss

                winrt::check_hresult(umpEndpoint->Initialize((LPCWSTR)(_deviceId.c_str()), &_mmcssTaskId, (IMidiCallback*)this));
            }

            std::cout << "MidiBidirectionalEndpointConnection::Start: Initialize Complete." << std::endl;
        }
        catch (winrt::hresult_error const& ex)
        {
            std::cout << "MidiBidirectionalEndpointConnection::Start: hresult exception on Initialize" << std::endl;
            std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return false;
        }

        MidiUmp32 ump;

        std::cout << "MidiBidirectionalEndpointConnection::Start: About to test sending a single UMP32" << std::endl;

        umpEndpoint->SendMidiMessage((PVOID)&ump, sizeof(MidiUmp32), 0);
        
        std::cout << "MidiBidirectionalEndpointConnection::Start: UMP32 sent" << std::endl;


        // todo: wire up the reader/writer to be able to call SendMidiMessage() etc.

      //  _messageReader = foo;
      //  _messageWriter = foo;

        return true;
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
