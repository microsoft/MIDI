// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSession.h"
#include "MidiSession.g.cpp"

#include "MidiEndpointConnection.h"
#include <midi_timestamp.h>
#include "midi_sdk_inproc_loopback_simulator.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::MidiSession MidiSession::CreateNewSession(hstring const& sessionName, winrt::Microsoft::Devices::Midi2::MidiSessionSettings const& settings)
    {
        // TODO: Call the service to create the session
        auto session = winrt::make_self<implementation::MidiSession>();

        session->Open();

        return *session;
    }
    void MidiSession::Open()
    {
        _isOpen = true;
    }
    bool MidiSession::IsOpen()
    {
        return _isOpen;
    }
    
    //winrt::Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Devices::Midi2::MidiEndpointConnection> MidiSession::Connections()
    winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Microsoft::Devices::Midi2::MidiEndpointConnection> MidiSession::Connections()
    {
        return _connections.GetView();
    }



    winrt::Microsoft::Devices::Midi2::MidiEndpointConnection MidiSession::ConnectToEndpoint(hstring const& midiEndpointId, winrt::Microsoft::Devices::Midi2::MidiEndpointConnectOptions const& options)
    {
        // TODO: Obviously this needs to return a real connection brokered by the service. 
        // Right now, it will accept any old Id just to enable testing


        // todo: need to convert to upper and remove leading/trailing spaces
        std::wstring normalizedEndpointId{ midiEndpointId };

        boost::algorithm::to_upper(normalizedEndpointId);
        boost::algorithm::trim(normalizedEndpointId);



        // verify we're not already connected to this. If we are, just return that connection.
        if (_connections.HasKey(normalizedEndpointId))
        {
            return _connections.Lookup(normalizedEndpointId);
        }


        // are we using the SDK Local loopback Ids for testing?

        if (normalizedEndpointId == MIDI_SDK_LOOPBACK_SIM1_ENDPOINT_ID || normalizedEndpointId == MIDI_SDK_LOOPBACK_SIM2_ENDPOINT_ID)
        {
            // this is a special case. We're using the SDK local (inproc) loopback for testing purposes
            auto endpoint = winrt::make_self<implementation::MidiEndpointConnection>((winrt::hstring)(normalizedEndpointId));



            // TODO: tell the endpoint connection to spin itself up

            _connections.Insert(normalizedEndpointId, *endpoint);

            return *endpoint;
        }
        else
        {
            // normal endpoint. Call the service API, set up the connection, and get the buffers
            auto endpoint = winrt::make_self<implementation::MidiEndpointConnection>((winrt::hstring)normalizedEndpointId);

            // TODO: tell the endpoint connection to spin itself up


            _connections.Insert(normalizedEndpointId, *endpoint);

            return *endpoint;
        }

    }
    void MidiSession::DisconnectFromEndpoint(hstring const& midiEndpointId)
    {
        // TODO: Disconnect from the service

        if (_connections.HasKey(midiEndpointId))
        {
            _connections.Remove(midiEndpointId);
        }
    }
    uint64_t MidiSession::GetMidiTimestamp()
    {
        return ::Microsoft::Devices::Midi2::Internal::Shared::GetCurrentMidiTimestamp();
    }
    uint64_t MidiSession::GetMidiTimestampFrequency()
    {
        return ::Microsoft::Devices::Midi2::Internal::Shared::GetMidiTimestampFrequency();
    }
    void MidiSession::Close()
    {
        // todo: need to clean up all connnections, disconnect from service, etc.

        std::for_each(
            begin(_connections), 
            end(_connections), 
            [](winrt::Windows::Foundation::Collections::IKeyValuePair<hstring, winrt::Microsoft::Devices::Midi2::MidiEndpointConnection> &kvp)
                { /* TODO */});

        _connections.Clear();

        _isOpen = false;
    }
}
