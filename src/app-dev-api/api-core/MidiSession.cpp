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
#include "MidiInputEndpointConnection.h"
#include "MidiOutputEndpointConnection.h"
#include "MidiBidirectionalEndpointConnection.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <iostream>




namespace winrt::Windows::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiSession MidiSession::CreateSession(
        hstring const& sessionName, 
        winrt::Windows::Devices::Midi2::MidiSessionSettings const& settings)
    {
        try
        {
            auto session = winrt::make_self<implementation::MidiSession>();


            // Connect to the MidiSrv abstraction

 //           std::cout << __FUNCTION__ << " creating and activating MidiSrv abstraction" << std::endl;

            // TODO: See if this umpEndpoint connection to the service is already open. If so, skip initialization

            session->SetName(sessionName);
            session->SetSettings(settings);

            if (session->InternalStart())
            {
                return *session;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            std::cout << __FUNCTION__ << ": hresult exception creating session" << std::endl;
            std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return nullptr;
        }
       
    }



    bool MidiSession::InternalStart()
    {
        try
        {
            // We're talking to the service, so use the MIDI Service abstraction, not a KS or other one
            _serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            // TODO: Not sure if service will need to provide the Id, or we can simply gen a GUID and send it up
            // that's why the assignment is in this function and not in CreateSession()
            _id = winrt::to_hstring(Windows::Foundation::GuidHelper::CreateNewGuid());

            _isOpen = true;
        }
        catch (winrt::hresult_error const& ex)
        {
            std::cout << __FUNCTION__ << ": hresult exception creating service abstraction" << std::endl;
            std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return false;
        }

        return true;
    }


    hstring MidiSession::NormalizeDeviceId(const hstring& deviceId)
    {
        std::wstring normalizedDeviceId{ deviceId };

        boost::algorithm::to_upper(normalizedDeviceId);
        boost::algorithm::trim(normalizedDeviceId);

        return (hstring)(normalizedDeviceId);
    }



    winrt::Windows::Devices::Midi2::MidiBidirectionalEndpointConnection MidiSession::ConnectBidirectionalEndpoint(
        hstring const& deviceId, 
        winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings)
    {
        auto normalizedDeviceId = NormalizeDeviceId(deviceId);
        auto endpointConnection = winrt::make_self<implementation::MidiBidirectionalEndpointConnection>();

        // generate internal endpoint Id
        auto guid = Windows::Foundation::GuidHelper::CreateNewGuid();
        auto endpointId = winrt::to_hstring(guid);

        endpointConnection->InternalSetId(endpointId);
        endpointConnection->InternalSetDeviceId(normalizedDeviceId);

        if (endpointConnection->InternalStart(_serviceAbstraction))
        {
            _connections.Insert((winrt::hstring)normalizedDeviceId, (const Windows::Devices::Midi2::MidiEndpointConnection)(*endpointConnection));

            return *endpointConnection;
        }
        else
        {
            OutputDebugString(L"" __FUNCTION__ ": WinRT Endpoint connection wouldn't start");

            // TODO: Cleanup

            return nullptr;
        }

    }


    winrt::Windows::Devices::Midi2::MidiOutputEndpointConnection MidiSession::ConnectOutputEndpoint(
        hstring const& deviceId, 
        winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings)
    {

        throw hresult_not_implemented();
    }


    winrt::Windows::Devices::Midi2::MidiInputEndpointConnection MidiSession::ConnectInputEndpoint(
        hstring const& deviceId, 
        winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings)
    {

        throw hresult_not_implemented();
    }



    void MidiSession::DisconnectEndpointConnection(hstring const& endpointConnectionId)
    {
        if (_connections.HasKey(endpointConnectionId))
        {
            // TODO: Disconnect from the service


            _connections.Remove(endpointConnectionId);
        }
        else
        {
            // endpoint already disconnected. No need to except or anything, just exit.
        }
    }

    //void MidiSession::DisconnectAllConnectionsForEndpoint(hstring const& deviceId)
    //{
    //}

   

    
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

        if (_serviceAbstraction != nullptr)
        {
            // TODO: Call any cleanup method on the service

            _serviceAbstraction = nullptr;
        }

        _isOpen = false;


    }


    MidiSession::~MidiSession()
    {
        if (_isOpen)
        {
            Close();
        }
    }
}
