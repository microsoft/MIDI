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

#include "MidiEndpointInformationEndpointListener.h"

#include "string_util.h"


namespace winrt::Windows::Devices::Midi2::implementation
{


    winrt::Windows::Devices::Midi2::MidiSession MidiSession::CreateSession(
        _In_ hstring const& sessionName,
        _In_ winrt::Windows::Devices::Midi2::MidiSessionSettings const& settings)
    {
        try
        {
            auto session = winrt::make_self<implementation::MidiSession>();

            session->SetName(sessionName);
            session->SetSettings(settings);

            if (session->InternalStart())
            {
                return *session;
            }
            else
            {
                // TODO: Returning nullptr on failure is not super useful per feedback. Consider throwing an hresult?

                return nullptr;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception initializing creating session. Service may be unavailable.", ex);

            // TODO: throwing an hresult exception here would be preferred vs returning null

            return nullptr;
        }
       
    }

    winrt::Windows::Devices::Midi2::MidiSession MidiSession::CreateSession(
        _In_ hstring const& sessionName)
    {
        return CreateSession(sessionName, winrt::Windows::Devices::Midi2::MidiSessionSettings::Default());
    }

    // Internal method called inside the API to connect to the abstraction. Called by the code which creates
    // the session instance
    bool MidiSession::InternalStart()
    {
        try
        {
            // We're talking to the service, so use the MIDI Service abstraction, not a KS or other one
            m_serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            // TODO: Not sure if service will need to provide the Id, or we can simply gen a GUID and send it up
            // that's why the assignment is in this function and not in CreateSession()
            m_id = winrt::to_hstring(Windows::Foundation::GuidHelper::CreateNewGuid());

            m_isOpen = true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception starting session. Service may be unavailable.", ex);

            return false;
        }

        return true;
    }


    hstring MidiSession::NormalizeDeviceId(_In_ const hstring& deviceId)
    {
        if (deviceId.empty()) return deviceId;

        return internal::ToUpperTrimmedHStringCopy(deviceId);
    }

    // Bidirectional ===========================================================================================================

    winrt::Windows::Devices::Midi2::MidiBidirectionalEndpointConnection MidiSession::ConnectBidirectionalEndpoint(
        _In_ hstring const& deviceId,
        _In_ winrt::Windows::Devices::Midi2::MidiBidirectionalEndpointOpenOptions const& /* options */,
        _In_ winrt::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings const& /*settings*/)
    {
        try
        {
            auto normalizedDeviceId = NormalizeDeviceId(deviceId);
            auto endpointConnection = winrt::make_self<implementation::MidiBidirectionalEndpointConnection>();

            // generate internal endpoint Id
            auto guid = Windows::Foundation::GuidHelper::CreateNewGuid();
            auto endpointId = winrt::to_hstring(guid);

            endpointConnection->InternalSetId(endpointId);
            endpointConnection->InternalSetDeviceId(normalizedDeviceId);

            if (endpointConnection->InternalInitialize(m_serviceAbstraction))
            {
                m_connections.Insert((winrt::hstring)normalizedDeviceId, (const Windows::Devices::Midi2::MidiEndpointConnection)(*endpointConnection));

                // TODO: This value needs to come from configuration for this endpoint. It comes from
                // the user settings property store, and then the open options
                bool autoHandleEndpointMessages = true;

                if (autoHandleEndpointMessages)
                {
                    //auto endpointInformationListener = winrt::make_self<implementation::MidiEndpointInformationEndpointListener>();

                    //endpointInformationListener->Id(L"auto_EndpointInformationListener");
                    //endpointInformationListener->Name(L"Automatic Endpoint Information Message Handler");
                    //endpointInformationListener->InputConnection(endpointConnection.as<IMidiInputConnection>());
                    //endpointInformationListener->Initialize();
                    //                   
                    //endpointConnection->MessageListeners().Append(*endpointInformationListener);
                }

                return *endpointConnection;
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"WinRT Endpoint connection wouldn't start");

                // TODO: Cleanup

                return nullptr;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception connecting to endpoint. Service or endpoint may be unavailable, or endpoint may not be the correct type.", ex);

            return nullptr;
        }
    }

    winrt::Windows::Devices::Midi2::MidiBidirectionalEndpointConnection MidiSession::ConnectBidirectionalEndpoint(
        _In_ hstring const& deviceId,
        _In_ winrt::Windows::Devices::Midi2::MidiBidirectionalEndpointOpenOptions const& options)
    {
        return ConnectBidirectionalEndpoint(deviceId, options, nullptr);
    }

    winrt::Windows::Devices::Midi2::MidiBidirectionalEndpointConnection MidiSession::ConnectBidirectionalEndpoint(
        _In_ hstring const& deviceId)
    {
        return ConnectBidirectionalEndpoint(deviceId, nullptr, nullptr);
    }




    // Output ===================================================================================================================

    winrt::Windows::Devices::Midi2::MidiOutputEndpointConnection MidiSession::ConnectOutputEndpoint(
        _In_ hstring const& /*deviceId*/,
        _In_ winrt::Windows::Devices::Midi2::MidiOutputEndpointOpenOptions const& /* options */,
        _In_ winrt::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings const& /*settings*/)
    {
        // TODO: Implement ConnectOutputEndpoint

        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiOutputEndpointConnection MidiSession::ConnectOutputEndpoint(
        _In_ hstring const& deviceId,
        _In_ winrt::Windows::Devices::Midi2::MidiOutputEndpointOpenOptions const& options)
    {
        return ConnectOutputEndpoint(deviceId, options, nullptr);
    }

    winrt::Windows::Devices::Midi2::MidiOutputEndpointConnection MidiSession::ConnectOutputEndpoint(
        _In_ hstring const& deviceId)
    {
        return ConnectOutputEndpoint(deviceId, nullptr, nullptr);
    }



    // Input ==================================================================================================================

    winrt::Windows::Devices::Midi2::MidiInputEndpointConnection MidiSession::ConnectInputEndpoint(
        _In_ hstring const& /*deviceId*/,
        _In_ winrt::Windows::Devices::Midi2::MidiInputEndpointOpenOptions const& /* options */,
        _In_ winrt::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings const& /*settings*/)
    {
        // TODO: Implement ConnectInputEndpoint

        throw hresult_not_implemented();
    }

    winrt::Windows::Devices::Midi2::MidiInputEndpointConnection MidiSession::ConnectInputEndpoint(
        _In_ hstring const& deviceId,
        _In_ winrt::Windows::Devices::Midi2::MidiInputEndpointOpenOptions const& options)
    {
        return ConnectInputEndpoint(deviceId, options, nullptr);
    }

    winrt::Windows::Devices::Midi2::MidiInputEndpointConnection MidiSession::ConnectInputEndpoint(
        _In_ hstring const& deviceId)
    {
        return ConnectInputEndpoint(deviceId, nullptr, nullptr);
    }





    void MidiSession::DisconnectEndpointConnection(_In_ hstring const& endpointConnectionId)
    {
        try
        {
            if (m_connections.HasKey(endpointConnectionId))
            {
                // todo: disconnect the endpoint from the service, call Close() etc.



                m_connections.Remove(endpointConnectionId);
            }
            else
            {
                // endpoint already disconnected. No need to except or anything, just exit.
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception disconnecting from endpoint.", ex);

            return;
        }
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

        m_connections.Clear();

        if (m_serviceAbstraction != nullptr)
        {
            // TODO: Call any cleanup method on the service

            m_serviceAbstraction = nullptr;
        }

        // Id is no longer valid, and session is not open. Clear these in case the client tries to use the held reference
        m_id.clear();
        m_isOpen = false;
        m_mmcssTaskId = 0;
    }



    MidiSession::~MidiSession()
    {
        if (m_isOpen)
        {
            Close();
        }
    }



}
