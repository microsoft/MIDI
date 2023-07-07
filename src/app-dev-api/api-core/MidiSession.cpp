// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSession.h"
#include "MidiSession.g.cpp"

#include "MidiEndpointConnection.h"
#include <midi_timestamp.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>

namespace winrt::Windows::Devices::Midi2::implementation
{
    hstring MidiSession::Id()
    {
        return _id;
    }

    winrt::Windows::Devices::Midi2::MidiSession MidiSession::CreateNewSession(hstring const& sessionName, winrt::Windows::Devices::Midi2::MidiSessionSettings const& settings)
    {
        // TODO: Call the service to create the session
        auto session = winrt::make_self<implementation::MidiSession>();

        //// TODO: Not sure if service will need to provide the Id, or we can simply gen a GUID and send it up
        //hstring id = winrt::to_hstring(Windows::Foundation::GuidHelper::CreateNewGuid());

        //session->SetIsOpen(true);
        //session->SetId(id);
        //session->SetName(sessionName);
        //session->SetSettings(settings);
        

        return *session;
    }

    bool MidiSession::IsOpen()
    {
        return _isOpen;
    }
    
    winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection> MidiSession::Connections()
    {
        return _connections.GetView();
    }

    winrt::Windows::Devices::Midi2::MidiEndpointConnection MidiSession::ConnectToEndpoint(hstring const& midiEndpointId, winrt::Windows::Devices::Midi2::MidiEndpointConnectOptions const& options)
    {
        // TODO: Obviously this needs to return a real connection brokered by the service. 
        // Right now, it will accept any old Id just to enable testing


        // cleanup the id
        std::wstring normalizedEndpointId{ midiEndpointId };

        boost::algorithm::to_upper(normalizedEndpointId);
        boost::algorithm::trim(normalizedEndpointId);



        // verify we're not already connected to this endpoint. If we are, just return that connection.
        if (_connections.HasKey((winrt::hstring)normalizedEndpointId))
        {
            return _connections.Lookup((winrt::hstring)normalizedEndpointId);
        }

        auto endpoint = winrt::make_self<implementation::MidiEndpointConnection>((winrt::hstring)normalizedEndpointId, _settings.UseMmcssThreads());

        endpoint->SetOptions(options);


        // TODO: Call the service API, set up the connection, and get the buffers


        // TODO: tell the endpoint connection to spin itself up to start listening





        _connections.Insert((winrt::hstring)normalizedEndpointId, (const Windows::Devices::Midi2::MidiEndpointConnection)(*endpoint));

        return *endpoint;
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
        return ::Windows::Devices::Midi2::Internal::Shared::GetCurrentMidiTimestamp();
    }
    
    uint64_t MidiSession::GetMidiTimestampFrequency()
    {
        return ::Windows::Devices::Midi2::Internal::Shared::GetMidiTimestampFrequency();
    }
    
    void MidiSession::Close()
    {
        // todo: need to clean up all connections, disconnect from service, etc.

        //std::for_each(
        //    begin(_connections), 
        //    end(_connections), 
        //    [](winrt::Windows::Foundation::Collections::IKeyValuePair<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection> &kvp)
        //        { 
        //            // TODO: close the Endpoint at the service, remove the buffers, etc.
        //            

        //        });

        _connections.Clear();

        // disconnect this session from the service completely


        // Id is no longer valid, and session is not open
        _id.clear();
        _isOpen = false;
    }


    MidiSession::~MidiSession()
    {
        if (_isOpen)
        {
         //   Close();
        }
    }
}
