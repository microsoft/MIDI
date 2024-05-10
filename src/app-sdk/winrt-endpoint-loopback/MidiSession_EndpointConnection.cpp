// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSession.h"
#include "MidiEndpointConnection.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::TryCreateEndpointConnection(
        winrt::hstring const& endpointDeviceId,
        bool const autoReconnect,
        midi2::IMidiEndpointConnectionSettings const& settings
    ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Creating Endpoint Connection");

        try
        {
            auto normalizedDeviceId = winrt::to_hstring(internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId.c_str()).c_str());
            auto endpointConnection = winrt::make_self<implementation::MidiEndpointConnection>();

            // generate internal endpoint Id
            auto connectionInstanceId = foundation::GuidHelper::CreateNewGuid();

            internal::LogInfo(__FUNCTION__, L"Initializing Endpoint Connection");

            if (endpointConnection->InternalInitialize(m_id, m_serviceAbstraction, connectionInstanceId, normalizedDeviceId, settings, autoReconnect))
            {
                m_connections.Insert(connectionInstanceId, *endpointConnection);

                // if we wnat to automatically reconnect this endpoint, we need to add it
                // to the reconnect list. That list takes the internal, not projected, type
                if (autoReconnect)
                {
                    m_connectionsForAutoReconnect.push_back(endpointConnection);

                    if (m_autoReconnectDeviceWatcher == nullptr)
                    {
                        StartEndpointWatcher();
                    }
                }

                return *endpointConnection;
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"WinRT Endpoint connection wouldn't initialize");

                // TODO: Cleanup

                return nullptr;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception connecting to endpoint. Service or endpoint may be unavailable, or endpoint may not be the correct type.", ex);

            return nullptr;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception creating endpoint connection");

            return nullptr;
        }
    }

    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::TryCreateEndpointConnection(
        winrt::hstring const& endpointDeviceId,
        bool autoReconnect
    ) noexcept
    {
        return TryCreateEndpointConnection(endpointDeviceId, autoReconnect, nullptr);
    }

    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::TryCreateEndpointConnection(
        winrt::hstring const& endpointDeviceId
    ) noexcept
    {
        return TryCreateEndpointConnection(endpointDeviceId, false, nullptr);
    }





    _Use_decl_annotations_
    void MidiSession::DisconnectEndpointConnection(
        winrt::guid const& endpointConnectionId
    ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Disconnect endpoint connection");

        try
        {
            if (m_connections.HasKey(endpointConnectionId))
            {
                // disconnect the endpoint from the service, call Close() etc.

                auto conn = m_connections.Lookup(endpointConnectionId);
                auto connSelf = winrt::get_self<implementation::MidiEndpointConnection>(conn);
                connSelf->InternalClose();

                m_connections.Remove(endpointConnectionId);
            }
            else
            {
                // endpoint already disconnected. No need to throw an exception or anything, just exit.
            }

            // remove our auto-reconnect endpoint pointer, if we have one
            auto it = std::find_if(
                m_connectionsForAutoReconnect.begin(),
                m_connectionsForAutoReconnect.end(),
                [&endpointConnectionId](const auto& x) { return x->ConnectionId() == endpointConnectionId; });

            if (it != m_connectionsForAutoReconnect.end())
            {
                m_connectionsForAutoReconnect.erase(it);
            }
            else
            {
                // may not have auto reconnect set for the connection, which is fine.
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

}