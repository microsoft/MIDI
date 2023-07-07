// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"
#include "MidiEndpointConnection.g.cpp"

#include "midi_service_interface.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    // internal constructor
    MidiEndpointConnection::MidiEndpointConnection(winrt::hstring deviceId, bool useMmcss)
    {
        _deviceId = deviceId;
        _useMmcss = useMmcss;
    }

    // internal method to start listening for incoming messages, enable processing outgoing messages, etc.
    void MidiEndpointConnection::Start()
    {
        // assuming bi-directional MIDI connection right now
        winrt::com_ptr<IMidiBiDi> umpEndpoint;

        // We're talking to the service, so use the MIDI Service abstraction, not a KS or other one
        auto abstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

        // get our BiDi endpoint
        winrt::check_hresult(abstraction->Activate(__uuidof(IMidiBiDi), (void **) &umpEndpoint));

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


    IFACEMETHODIMP MidiEndpointConnection::Callback(PVOID Data, UINT Size, LONGLONG Position)
    {
        // TODO: process incoming messages / fire event

        return S_OK;
    }

    hstring MidiEndpointConnection::GetDeviceSelector()
    {
        // TODO: get this selector from Julia. It will return all UMP Endpoints the system recognizes
        // Additional selectors will be made available through the SDK, as they are just string queries
        // Right now, this is the selector from Gary's MidiKsEnum code

        return L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";
    }

    hstring MidiEndpointConnection::DeviceId()
    {
        return _deviceId;
    }

    winrt::Windows::Devices::Midi2::MidiMessageReader MidiEndpointConnection::GetMessageReader()
    {
        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiMessageWriter MidiEndpointConnection::GetMessageWriter()
    {
        throw hresult_not_implemented();
    }

    winrt::event_token MidiEndpointConnection::MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler)
    {
        throw hresult_not_implemented();
    }

    void MidiEndpointConnection::MessagesReceived(winrt::event_token const& token) noexcept
    {
        throw hresult_not_implemented();
    }
}
