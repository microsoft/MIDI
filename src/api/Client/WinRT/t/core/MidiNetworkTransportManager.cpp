// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkTransportManager.h"
#include "Transports.Network.MidiNetworkTransportManager.g.cpp"

#include "..\..\api\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

#include "MidiNetworkAdvertisedHost.h"

#include "MidiNetworkHostCreationConfig.h"
#include "MidiNetworkHostCreationResponse.h"

#include "MidiNetworkHostUpdateResponse.h"

#include "MidiNetworkHostRemovalConfig.h"
#include "MidiNetworkHostRemovalResponse.h"

#include "MidiNetworkClientConnectConfig.h"
#include "MidiNetworkClientConnectResponse.h"
#include "MidiNetworkClientDisconnectConfig.h"
#include "MidiNetworkClientDisconnectResponse.h"

#include "MidiReporting.h"

#include "MidiServiceTransportPluginConfigManager.h"

#include "MidiNetworkConfiguredHost.h"
#include "MidiNetworkConfiguredClient.h"

#include "MidiNetworkRemoteClientApprovalConfig.h"
#include "MidiNetworkRemoteClientApprovalResponse.h"

//#include <pplawait.h>

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    bool MidiNetworkTransportManager::IsTransportAvailable() noexcept
    {
        auto transports = rpt::MidiReporting::GetInstalledTransportPlugins();

        for (auto const& transport: transports)
        {
            if (transport.TransportId() == TransportId())
            {
                return true;
            }
        }

        return false;
    }


    _Use_decl_annotations_ 
    foundation::IAsyncOperation<network::MidiNetworkHostUpdateResponse> MidiNetworkTransportManager::StartNetworkHostAsync(winrt::guid const& hostId)
    {
        auto result = winrt::make_self<MidiNetworkHostUpdateResponse>();
        result->InternalSetHostId(hostId);

        try
        {
            midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());

            command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_START_HOST);
            command.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                winrt::to_hstring(hostId));

            co_await winrt::resume_background();

            // this could take a few since it closes all the connections synchronously in the service
            auto response = midi2::ServiceConfig::MidiServiceTransportPluginConfigManager::SendCommand(command);

        
            if (response.Status() == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                // TODO: Get actual error code from json
                result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::NoErrorInformationAvailable, response.ServiceErrorMessage());
            }

            co_return *result;
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to stop network host. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: Get actual error code
            result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::NoErrorInformationAvailable, ex.message());

            co_return *result;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to stop network host. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            // TODO: Get actual error code
            result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::NoErrorInformationAvailable, L"General exception");

            co_return *result;
        }
    }
    
    _Use_decl_annotations_ 
    foundation::IAsyncOperation<network::MidiNetworkHostUpdateResponse> MidiNetworkTransportManager::StopNetworkHostAsync(winrt::guid const& hostId)
    {
        auto result = winrt::make_self<MidiNetworkHostUpdateResponse>();
        result->InternalSetHostId(hostId);

        try
        {
            midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());

            command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_STOP_HOST);
            command.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                winrt::to_hstring(hostId));

            co_await winrt::resume_background();

            // this could take a few since it closes all the connections synchronously in the service
            auto response = midi2::ServiceConfig::MidiServiceTransportPluginConfigManager::SendCommand(command);

            if (response.Status() == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                // TODO: Get actual error code
                result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::NoErrorInformationAvailable, response.ServiceErrorMessage());
            }

            co_return *result;
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to stop network host. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: Get actual error code
            result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::NoErrorInformationAvailable, ex.message());

            co_return *result;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to stop network host. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            // TODO: Get actual error code
            result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::NoErrorInformationAvailable, L"General exception");

            co_return *result;
        }
    }


    collections::IVectorView<network::MidiNetworkConfiguredHost> MidiNetworkTransportManager::GetConfiguredHosts() noexcept
    {
        auto results = winrt::single_threaded_vector<network::MidiNetworkConfiguredHost>();

        try
        {
            midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());
            command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_HOSTS);

            auto response = midi2::ServiceConfig::MidiServiceTransportPluginConfigManager::SendCommand(command);

            if (response.Status() == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
            {
                auto responseJson = response.ResponseJson();

                if (responseJson.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HOSTS_ARRAY_KEY))
                {
                    auto hostsArray = responseJson.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HOSTS_ARRAY_KEY);

                    for (auto const& entry : hostsArray)
                    {
                        auto entryObject = entry.GetObject();

                        if (entryObject != nullptr)
                        {
                            auto host = winrt::make_self<MidiNetworkConfiguredHost>();

                            host->InternalInitialize(
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_IS_ENABLED_KEY, false),
                                winrt::guid(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONFIG_ID_KEY, L"")),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NAME_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_PRODUCT_INSTANCE_ID_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_SERVICE_INSTANCE_NAME_KEY, L""),
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HAS_STARTED_KEY, false),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_ADDRESS_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_PORT_KEY, L""),
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CREATE_MIDI1_PORTS_KEY, false)
                            );

                            results.Append(*host);
                        }
                    }
                }
                else
                {
                    // no response array
                }
            }
            else
            {
                // failed
            }
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to get configured hosts. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to get configured hosts. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return results.GetView();
    }


    collections::IVectorView<network::MidiNetworkConfiguredClient> MidiNetworkTransportManager::GetConfiguredClients() noexcept
    {
        auto results = winrt::single_threaded_vector<network::MidiNetworkConfiguredClient>();

        try
        {
            midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());
            command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS);

            auto response = midi2::ServiceConfig::MidiServiceTransportPluginConfigManager::SendCommand(command);


            if (response.Status() == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
            {
                auto responseJson = response.ResponseJson();

                if (responseJson.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CLIENTS_ARRAY_KEY))
                {
                    auto hostsArray = responseJson.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CLIENTS_ARRAY_KEY);

                    for (auto const& entry : hostsArray)
                    {
                        auto entryObject = entry.GetObject();

                        if (entryObject != nullptr)
                        {
                            auto client = winrt::make_self<MidiNetworkConfiguredClient>();

                            client->InternalInitialize(
                                winrt::guid(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CONFIG_ID_KEY, L"")),
                                winrt::guid(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_HOST_ID_KEY, L"")),
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_SESSION_ACTIVE_KEY, false),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_REMOTE_ADDRESS_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_REMOTE_PORT_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_LOCAL_ADDRESS_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_LOCAL_PORT_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_UMP_ENDPOINT_ID_KEY, L""),

                                static_cast<uint64_t>(entryObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CURRENT_LATENCY_KEY, 0)),
                                static_cast<uint32_t>(entryObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_RETRANSMIT_COUNT_KEY, 0)),
                                static_cast<uint32_t>(entryObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_RETRANSMIT_REQUEST_COUNT_KEY, 0)),

                                static_cast<uint64_t>(entryObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_NETWORK_PACKETS_SENT_KEY, 0)),
                                static_cast<uint64_t>(entryObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_NETWORK_PACKETS_RECEIVED_KEY, 0))
                                );

                            results.Append(*client);
                        }
                    }
                }
                else
                {
                    // no response array
                }
            }
            else
            {
                // failed
            }
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to get configured clients. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to get configured clients. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return results.GetView();
    }


    collections::IVectorView<network::MidiNetworkPendingRemoteClient> MidiNetworkTransportManager::GetPendingRemoteClients() noexcept
    {

        // TODO

        return nullptr;     // temp

    }






    // TODO: Not yet really async
    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkHostCreationResponse> MidiNetworkTransportManager::CreateNetworkHostAsync(
        network::MidiNetworkHostCreationConfig const& creationConfig) noexcept
    {
        auto result = winrt::make_self<MidiNetworkHostCreationResponse>();

    //    co_await winrt::resume_background();

        try
        {
            // TODO. This doesn't do everything sync in the service so needs to change
            auto createResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(creationConfig);

            if (createResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                // todo: get actual error code
                result->InternalSetError(network::MidiNetworkHostCreationErrorCode::NoErrorInformationAvailable, createResponse.ServiceErrorMessage());
            }

            co_return *result;
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to create network host. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: Get actual error code
            result->InternalSetError(network::MidiNetworkHostCreationErrorCode::NoErrorInformationAvailable, ex.message());

            co_return *result;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to create network host. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            // TODO: Get actual error code
            result->InternalSetError(network::MidiNetworkHostCreationErrorCode::NoErrorInformationAvailable, L"General exception.");

            co_return *result;
        }
    }

    // TODO: Not yet really async
    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkHostRemovalResponse>
    MidiNetworkTransportManager::RemoveNetworkHostAsync(
        network::MidiNetworkHostRemovalConfig const& removalConfig) noexcept
    {
        auto result = winrt::make_self<MidiNetworkHostRemovalResponse>();

        // TODO: Get actual error code
        result->InternalSetError(network::MidiNetworkHostRemovalErrorCode::NoErrorInformationAvailable, L"Not yet implemented");

        co_return *result;
    }



    // TODO: not yet really async
    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkClientConnectResponse>
    MidiNetworkTransportManager::ConnectNetworkClientAsync(
        network::MidiNetworkClientConnectConfig const& creationConfig) noexcept
    {
        // TODO: Right now this is only doing direct connects, not MDNS connects
        // TODO: There's no endpoint name in the config

        svc::MidiServiceTransportCommand cmd(MidiNetworkTransportManager::TransportId());
        cmd.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_DIRECT);
        cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER, winrt::to_hstring(creationConfig.ClientId()));
        cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_ADDRESS, creationConfig.MatchCriteria().DirectHostNameOrIPAddress());
        cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_PORT, winrt::to_hstring(creationConfig.MatchCriteria().DirectPort()));
        cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME, creationConfig.UmpEndpointName());

        auto createResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

        auto result = winrt::make_self<MidiNetworkClientConnectResponse>();

        if (createResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
        {
            result->InternalSetSuccess();
        }
        else
        {
            // TODO: Get actual error code
            result->InternalSetError(network::MidiNetworkClientConnectErrorCode::NoErrorInformationAvailable, createResponse.ServiceErrorMessage());
        }

        co_return *result;
    }
    

    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkClientDisconnectResponse>
    MidiNetworkTransportManager::DisconnectNetworkClientAsync(
        network::MidiNetworkClientDisconnectConfig const& disconnectConfig) noexcept
    {
        auto result = winrt::make_self<MidiNetworkClientDisconnectResponse>();

        try
        {
            svc::MidiServiceTransportCommand cmd(MidiNetworkTransportManager::TransportId());
            cmd.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT);
            cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER, winrt::to_hstring(disconnectConfig.ClientId()));

            auto response = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

            if (response.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                // TODO: Get actual error code
                result->InternalSetError(network::MidiNetworkClientDisconnectErrorCode::NoErrorInformationAvailable, response.ServiceErrorMessage());
            }

            co_return *result;
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to disconnect network client. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: Get actual error code
            result->InternalSetError(network::MidiNetworkClientDisconnectErrorCode::NoErrorInformationAvailable, ex.message());

            co_return *result;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to disconnect network client. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            // TODO: Get actual error code
            result->InternalSetError(network::MidiNetworkClientDisconnectErrorCode::NoErrorInformationAvailable, L"General exception.");

            co_return *result;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkRemoteClientApprovalResponse> MidiNetworkTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(
        network::MidiNetworkRemoteClientApprovalConfig const& approvalConfig) noexcept
    {
        UNREFERENCED_PARAMETER(approvalConfig);

        auto response = winrt::make_self<MidiNetworkRemoteClientApprovalResponse>();

        try
        {
            svc::MidiServiceTransportCommand cmd(MidiNetworkTransportManager::TransportId());

            co_await resume_background();


            // TODO: Set verb based on approvalConfig.ApprovalAction()

            cmd.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT);
            //cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER, winrt::to_hstring(approvalConfig.ClientId()));



            // TODO: Additional arguments












            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

            if (serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                response->InternalSetSuccess();
            }
            else
            {
                // TODO: Get actual error code
                response->InternalSetError(static_cast<network::MidiNetworkRemoteClientApprovalErrorCode>(serviceResponse.ServiceErrorCode()), serviceResponse.ServiceErrorMessage());
            }

            co_return *response;
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to disconnect network client. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: Get actual error code
            response->InternalSetError(network::MidiNetworkRemoteClientApprovalErrorCode::NoErrorInformationAvailable, ex.message());

            co_return *response;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to disconnect network client. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            // TODO: Get actual error code
            response->InternalSetError(network::MidiNetworkRemoteClientApprovalErrorCode::NoErrorInformationAvailable, L"General exception.");

            co_return *response;
        }


    }







    winrt::hstring MidiNetworkTransportManager::MidiNetworkUdpDnsServiceType() noexcept
    { 
        return L"_midi2._udp"; 
    }

    winrt::hstring MidiNetworkTransportManager::MidiNetworkUdpDnsDomain() noexcept
    { 
        return L"local"; 
    }

    winrt::hstring MidiNetworkTransportManager::MidiNetworkUdpDnsSdQueryString() noexcept
    {
        // protocol guid from https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/enumerate-devices-over-a-network

        return
            L"System.Devices.AepService.ProtocolId:={4526e8c1-8aac-4153-9b16-55e86ada0e54} AND " \
            L"System.Devices.Dnssd.ServiceName:=\"" + MidiNetworkTransportManager::MidiNetworkUdpDnsServiceType() + L"\" AND " \
            L"System.Devices.Dnssd.Domain:=\"" + MidiNetworkTransportManager::MidiNetworkUdpDnsDomain() + L"\"";
    }

    enumeration::DeviceInformationKind MidiNetworkTransportManager::MidiNetworkUdpDnsSdDeviceInformationKind() noexcept
    {
        return enumeration::DeviceInformationKind::AssociationEndpointService;
    }


    collections::IVector<winrt::hstring> MidiNetworkTransportManager::MidiNetworkUdpDnsSdQueryAdditionalProperties() noexcept
    {
        auto props = winrt::single_threaded_vector<winrt::hstring>();

        // https://learn.microsoft.com/en-us/windows/win32/properties/props-system-devices-dnssd-domain
        props.Append(L"System.Devices.AepService.ProtocolId");  // guid
        props.Append(L"System.Devices.Dnssd.HostName");         // string
        props.Append(L"System.Devices.Dnssd.FullName");         // string
        props.Append(L"System.Devices.Dnssd.ServiceName");      // string
        props.Append(L"System.Devices.Dnssd.Domain");           // string
        props.Append(L"System.Devices.Dnssd.InstanceName");     // string
        props.Append(L"System.Devices.IpAddress");              // multivalue string
        props.Append(L"System.Devices.Dnssd.PortNumber");       // uint16_t
        props.Append(L"System.Devices.Dnssd.TextAttributes");   // multivalue string

        return props;
    }



	// this method takes way too long. This needs to be changed to pull from the cache in the 
	// service via the json methods.

    collections::IVectorView<network::MidiNetworkAdvertisedHost> MidiNetworkTransportManager::GetAdvertisedHosts() noexcept
    {
        auto results = winrt::single_threaded_vector<network::MidiNetworkAdvertisedHost>();

        try
        {
            auto entries = enumeration::DeviceInformation::FindAllAsync(
                MidiNetworkUdpDnsSdQueryString(), 
                MidiNetworkUdpDnsSdQueryAdditionalProperties(),
                MidiNetworkUdpDnsSdDeviceInformationKind()
            ).get();

            if (entries && entries.Size() > 0)
            {
                for (auto const& entry : entries)
                {
                    auto host = winrt::make_self<network::implementation::MidiNetworkAdvertisedHost>();

                    host->InternalUpdateFromDeviceInformation(entry);

                    results.Append(*host);
                }

            }

            // empty collection if nothing found
            return results.GetView();
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(ex.code());

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to list advertised hosts. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to list advertised hosts. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return results.GetView();
    }
}
