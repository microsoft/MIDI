// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSession.h"
#include "MidiEndpointConnection.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::CreateEndpointConnection(
        winrt::hstring const& endpointDeviceId,
        bool const autoReconnect,
        midi2::IMidiEndpointConnectionSettings const& settings
    ) noexcept
    {
        try
        {
            auto normalizedDeviceId = winrt::to_hstring(internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId.c_str()).c_str());
            auto endpointConnection = winrt::make_self<implementation::MidiEndpointConnection>();

            // generate internal endpoint Id
            auto connectionInstanceId = foundation::GuidHelper::CreateNewGuid();

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
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"WinRT Endpoint connection wouldn't initialize", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );
                // TODO: Cleanup

                return nullptr;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception connecting to endpoint. Service or endpoint may be unavailable, or endpoint may not be the correct type.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return nullptr;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception creating endpoint connection", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }
    }

    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::CreateEndpointConnection(
        winrt::hstring const& endpointDeviceId,
        bool autoReconnect
    ) noexcept
    {
        return CreateEndpointConnection(endpointDeviceId, autoReconnect, nullptr);
    }

    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::CreateEndpointConnection(
        winrt::hstring const& endpointDeviceId
    ) noexcept
    {
        return CreateEndpointConnection(endpointDeviceId, false, nullptr);
    }





    _Use_decl_annotations_
    void MidiSession::DisconnectEndpointConnection(
        winrt::guid const& endpointConnectionId
    ) noexcept
    {
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
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception disconnecting from endpoint.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unknown exception disconnecting endpoint connection.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }

}