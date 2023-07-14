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
    winrt::Windows::Devices::Midi2::MidiSession MidiSession::CreateSession(hstring const& sessionName, winrt::Windows::Devices::Midi2::MidiSessionSettings const& settings)
    {
        try
        {
            auto session = winrt::make_self<implementation::MidiSession>();


            // Connect to the MidiSrv abstraction

 //           std::cout << __FUNCTION__ << " creating and activating MidiSrv abstraction" << std::endl;

            // TODO: See if this umpEndpoint connection to the service is already open. If so, skip initialization

            session->SetName(sessionName);
            session->SetSettings(settings);

            if (session->Start())
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



    bool MidiSession::Start()
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



    bool MidiSession::ActivateMidiStream(const IID &iid, void** iface)
    {
        try
        {
 //           std::cout << __FUNCTION__ << ": activating BiDi" << std::endl;

            winrt::check_hresult(_serviceAbstraction->Activate(iid, iface));

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            std::cout << __FUNCTION__ << ": hresult exception on Service Abstraction Activate (service may not be installed or running or endpoint type is wrong)" << std::endl;
            std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return false;
        }
    }

    template<class TInterface>
    std::shared_ptr<internal::InternalMidiDeviceConnection> MidiSession::GetOrCreateAndInitializeDeviceConnection(std::string normalizedDeviceId, winrt::com_ptr<TInterface> iface)
    {
        std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection;

//        std::cout << __FUNCTION__ << ": looking for existing connection" << std::endl;

        if (_internalDeviceConnections.find(normalizedDeviceId) != _internalDeviceConnections.end())
        {
            // device connection already exists. Use it (and add another reference)
            return _internalDeviceConnections[normalizedDeviceId];
        }
        else
        {
            // device connection doesn't exist. Spin up a new one

       //     deviceConnection = std::make_shared<internal::InternalMidiDeviceConnection>();

            try
            {
//                std::cout << __FUNCTION__ << ": initializing BiDi" << std::endl;

                if (_useMmcss)
                {
                    //// TODO: Need to handle the output only case which has no callback
                    //winrt::check_hresult(iface->Initialize(
                    //    (LPCWSTR)(normalizedDeviceId.c_str()), 
                    //    &_mmcssTaskId, 
                    //    (IMidiCallback*)(deviceConnection.get())
                    //));
                }
                else
                {
                    // TODO: Need another call or parameter set for the abstraction to tell it not to use mmcss
                    // TODO: Need to handle the output only case which has no callback

                //    winrt::check_hresult(iface->Initialize((LPCWSTR)(normalizedDeviceId.c_str(), &_mmcssTaskId, (IMidiCallback*)deviceConnection));
                }

                // store this in our internal collection
//                _internalDeviceConnections[normalizedDeviceId] = deviceConnection;

            }
            catch (winrt::hresult_error const& ex)
            {
                std::cout << __FUNCTION__ << " hresult exception on Initialize" << std::endl;
                std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
                std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

                return nullptr;
            }
        }

    }


    winrt::Windows::Devices::Midi2::MidiBidirectionalEndpointConnection MidiSession::ConnectBidirectionalEndpoint(
        hstring const& deviceId, 
        hstring const& tag,
        winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings)
    {
        winrt::com_ptr<IMidiBiDi> umpEndpointInterface{};

        // cleanup the id
        auto normalizedDeviceId = NormalizeDeviceId(deviceId);

        // Activate the BiDi endpoint for this device. Will fail if the device is not a BiDi device
        if (!ActivateMidiStream(__uuidof(IMidiBiDi), (void**)&umpEndpointInterface))
        {
            return nullptr;
        }

        // Create the new endpoint and then get a com_ptr to the WinRT endpoint implementation type
        auto endpointConnection = winrt::make_self<implementation::MidiBidirectionalEndpointConnection>();


        auto guid = Windows::Foundation::GuidHelper::CreateNewGuid();
        auto endpointId = winrt::to_hstring(guid);

        endpointConnection->InternalSetId(endpointId);


        // internal tracking of the master connection for this endpoint
        //std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection = 
        //    GetOrCreateAndInitializeDeviceConnection<IMidiBiDi>(winrt::to_string(normalizedDeviceId), umpEndpointInterface);


        // this is all really messy right now
        std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection = std::make_shared<internal::InternalMidiDeviceConnection>();
        deviceConnection->DeviceId = normalizedDeviceId;
        deviceConnection->BidirectionalConnection = umpEndpointInterface;

        auto endpointConnectionRef = endpointConnection.get();


        // Right now, without a separate listener class, this creates a complete stream for each
        // connected endpoint, which will get expensive when folks create 16 of them for an endpoint
        // so they can have something akin to MIDI 1.0 ports (per group)
        // COM class in C++/WinRT example. This is the way to go so as not to surface an internal WinRT class.
        // https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/author-coclasses
        //
        // Use an approach like what I had planned, but properly implement a COM coclass to service the callback
        // and call a non-COM callback in the EndpointConnection classes

        try
        {
            // TODO: Need to handle the output only case which has no callback
            winrt::check_hresult(umpEndpointInterface->Initialize(
                (LPCWSTR)(normalizedDeviceId.c_str()),
                &_mmcssTaskId,
                (IMidiCallback*)(endpointConnectionRef)
            ));
        }
        catch (winrt::hresult_error const& ex)
        {
            std::cout << __FUNCTION__ << " hresult exception on Initialize endpoint with callback" << std::endl;
            std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return nullptr;
        }



        if (endpointConnection->Start(deviceConnection))
        {
            _connections.Insert((winrt::hstring)normalizedDeviceId, (const Windows::Devices::Midi2::MidiEndpointConnection)(*endpointConnection));

            return *endpointConnection;
        }
        else
        {
            std::cout << __FUNCTION__ << ": WinRT Endpoint connection wouldn't start " << std::endl;

            // TODO: endpointConnection wouldn't start

            // TODO: Cleanup

            return nullptr;
        }

    }


    winrt::Windows::Devices::Midi2::MidiOutputEndpointConnection MidiSession::ConnectOutputEndpoint(hstring const& deviceId, hstring const& tag, winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings)
    {
        winrt::com_ptr<IMidiOut> umpEndpoint{};

        // cleanup the id
        auto normalizedDeviceId = NormalizeDeviceId(deviceId);


        throw hresult_not_implemented();
    }


    winrt::Windows::Devices::Midi2::MidiInputEndpointConnection MidiSession::ConnectInputEndpoint(hstring const& deviceId, hstring const& tag, winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings)
    {
        winrt::com_ptr<IMidiIn> umpEndpoint{};

        // cleanup the id
        auto normalizedDeviceId = NormalizeDeviceId(deviceId);


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
