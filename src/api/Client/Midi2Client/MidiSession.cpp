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

#include "string_util.h"


namespace winrt::Windows::Devices::Midi2::implementation
{

    _Use_decl_annotations_
    midi2::MidiSession MidiSession::CreateSession(
        winrt::hstring const& sessionName,
        midi2::MidiSessionSettings const& settings
        ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L" Session create ");

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
                return nullptr;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception initializing creating session. Service may be unavailable.", ex);

            return nullptr;
        }
       
    }

    _Use_decl_annotations_
    midi2::MidiSession MidiSession::CreateSession(
        winrt::hstring const& sessionName
        ) noexcept
    {
        return CreateSession(sessionName, MidiSessionSettings());
    }

    // Internal method called inside the API to connect to the abstraction. Called by the code which creates
    // the session instance
    bool MidiSession::InternalStart()
    {
        internal::LogInfo(__FUNCTION__, L" Start Session ");

        try
        {
            // We're talking to the service, so use the MIDI Service abstraction, not a KS or other one
            m_serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            if (m_serviceAbstraction != nullptr)
            {
                // TODO: Not sure if service will need to provide the Id, or we can simply gen a GUID and send it up
                // that's why the assignment is in this function and not in CreateSession()
                m_id = foundation::GuidHelper::CreateNewGuid();

                m_isOpen = true;

                // create the virtual device manager

//                auto virtualDeviceManager = winrt::make_self<implementation::MidiVirtualDeviceManager>();
//                if (virtualDeviceManager != nullptr)
//                {
//                    virtualDeviceManager->Initialize(m_serviceAbstraction);
//
//                    m_virtualDeviceManager = *virtualDeviceManager;
//                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Error starting session. Service abstraction is nullptr.");

                return false;
            }

        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception starting session. Service may be unavailable.", ex);

            return false;
        }

        return true;
    }

    _Use_decl_annotations_
    winrt::hstring MidiSession::NormalizeDeviceId(const winrt::hstring& endpointDeviceId)
    {
        if (endpointDeviceId.empty()) return endpointDeviceId;

        return internal::ToUpperTrimmedHStringCopy(endpointDeviceId);
    }




    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::CreateEndpointConnection(
        winrt::hstring const& endpointDeviceId,
        midi2::MidiEndpointConnectionOptions const& options,
        midi2::IMidiEndpointDefinedConnectionSettings const& /*settings*/
        ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L" Creating Endpoint Connection");

        try
        {
            auto normalizedDeviceId = NormalizeDeviceId(endpointDeviceId);
            auto endpointConnection = winrt::make_self<implementation::MidiEndpointConnection>();

            // default options
            midi2::MidiEndpointConnectionOptions fixedOptions{ nullptr };

            if (options != nullptr)
            {
                fixedOptions = options;
            }

            // generate internal endpoint Id
            auto connectionInstanceId = foundation::GuidHelper::CreateNewGuid();

            if (endpointConnection->InternalInitialize(m_serviceAbstraction, connectionInstanceId, normalizedDeviceId, fixedOptions))
            {
                m_connections.Insert(connectionInstanceId, *endpointConnection);

                return *endpointConnection;
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L" WinRT Endpoint connection wouldn't start");

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

    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::CreateEndpointConnection(
        winrt::hstring const& endpointDeviceId,
        midi2::MidiEndpointConnectionOptions const& options
        ) noexcept
    {
        return CreateEndpointConnection(endpointDeviceId, options, nullptr);
    }

    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::CreateEndpointConnection(
        winrt::hstring const& endpointDeviceId
        ) noexcept
    {
        return CreateEndpointConnection(endpointDeviceId, nullptr, nullptr);
    }


    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::CreateVirtualDeviceAndConnection(
        winrt::hstring /*endpointName*/,
        winrt::hstring /*endpointDeviceInstanceId*/
    ) noexcept
    {
        // TODO

        return nullptr;
    }





    _Use_decl_annotations_
    void MidiSession::DisconnectEndpointConnection(
        winrt::guid const& endpointConnectionId
        ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L" Disconnect endpoint connection ");

        try
        {
            if (m_connections.HasKey(endpointConnectionId))
            {
                // todo: disconnect the endpoint from the service, call Close() etc.

                m_connections.Lookup(endpointConnectionId).as<foundation::IClosable>().Close();

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
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L" Unknown exception disconnecting endpoint connection");
        }
    }



    void MidiSession::Close() noexcept
    {
        internal::LogInfo(__FUNCTION__, L" Closing session");

        if (!m_isOpen) return;

        try
        {
            if (m_serviceAbstraction != nullptr)
            {
                // TODO: Call any cleanup method on the service
                for (auto connection : m_connections)
                {
                    // close the one connection
                    connection.Value().as<foundation::IClosable>().Close();
                }

                m_serviceAbstraction = nullptr;
            }

            m_connections.Clear();

            // Id is no longer valid, and session is not open. Clear these in case the client tries to use the held reference
            //m_id.clear();
            m_isOpen = false;
            m_mmcssTaskId = 0;
            
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception on close");
        }

    }


    MidiSession::~MidiSession() noexcept
    {
        if (m_isOpen)
        {
            Close();
        }
    }



}
