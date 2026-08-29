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
#include "MidiNetworkTransportSettings.h"

#include "midi_network_port_picker.h"

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
#include "MidiNetworkHostConnection.h"
#include "MidiNetworkConfiguredClient.h"
#include "MidiNetworkPendingRemoteClient.h"

#include "MidiNetworkRemoteClientApprovalConfig.h"
#include "MidiNetworkRemoteClientApprovalResponse.h"

#include "MidiNetworkRemoteClientDisconnectConfig.h"
#include "MidiNetworkRemoteClientDisconnectResponse.h"

//#include <pplawait.h>

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    namespace
    {
        // winrt::guid's string constructor validates length, separators and every hex digit, and
        // accepts both the braced and unbraced forms. It throws std::invalid_argument rather than
        // an hresult_error, so it needs its own catch. Entry identifiers come from the
        // configuration file, where a user can type anything, and one bad entry used to abort an
        // entire enumeration and surface as an empty list rather than an error.
        bool TryParseGuid(_In_ winrt::hstring const& value, _Out_ winrt::guid& result) noexcept
        {
            result = winrt::guid{};

            if (value.empty())
            {
                return false;
            }

            try
            {
                result = winrt::guid{ std::wstring_view{ value } };

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        network::MidiNetworkClientEntryState EntryStateFromString(_In_ winrt::hstring const& value) noexcept
        {
            if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_LIVE)
            {
                return network::MidiNetworkClientEntryState::Active;
            }

            if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_FAILED)
            {
                return network::MidiNetworkClientEntryState::Failed;
            }

            if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_UNAVAILABLE)
            {
                return network::MidiNetworkClientEntryState::Unavailable;
            }

            return network::MidiNetworkClientEntryState::Pending;
        }

        network::MidiNetworkRemoteClientPolicy RemoteClientPolicyFromString(_In_ winrt::hstring const& value) noexcept
        {
            if (value == MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL)
            {
                return network::MidiNetworkRemoteClientPolicy::RequireApproval;
            }

            return network::MidiNetworkRemoteClientPolicy::AllowAny;
        }

        // The service turns a submitted host definition into a live host on its creator worker,
        // so the host is not up yet when the configuration update call returns.
        constexpr uint32_t HostStartPollAttempts{ 40 };
        constexpr std::chrono::milliseconds HostStartPollInterval{ 250 };

        bool ConfiguredHostHasStarted(
            _In_ collections::IVectorView<network::MidiNetworkConfiguredHost> const& hosts,
            _In_ winrt::guid const& hostId) noexcept
        {
            if (hosts == nullptr)
            {
                return false;
            }

            for (auto const& host : hosts)
            {
                if (host != nullptr && host.HostId() == hostId)
                {
                    return host.HasStarted();
                }
            }

            return false;
        }
    }

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
    foundation::IAsyncOperation<network::MidiNetworkHostUpdateResponse> MidiNetworkTransportManager::StartNetworkHostAsync(winrt::guid const& hostId) noexcept
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

            auto response = midi2::ServiceConfig::MidiServiceTransportPluginConfigManager::SendCommand(command);

        
            if (response.Status() == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                result->InternalSetError(static_cast<network::MidiNetworkHostUpdateErrorCode>(response.ServiceErrorCode()), response.ServiceErrorMessage());
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
                TraceLoggingWideString(L"Unable to start network host. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::ClientApiException, ex.message());

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
                TraceLoggingWideString(L"Unable to start network host. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::ClientApiException, internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

            co_return *result;
        }
    }
    
    _Use_decl_annotations_ 
    foundation::IAsyncOperation<network::MidiNetworkHostUpdateResponse> MidiNetworkTransportManager::StopNetworkHostAsync(winrt::guid const& hostId) noexcept
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
                result->InternalSetError(static_cast<network::MidiNetworkHostUpdateErrorCode>(response.ServiceErrorCode()), response.ServiceErrorMessage());
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
            result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::ClientApiException, ex.message());

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
            result->InternalSetError(network::MidiNetworkHostUpdateErrorCode::ClientApiException, internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

            co_return *result;
        }
    }


    network::MidiNetworkTransportSettings MidiNetworkTransportManager::GetTransportSettings() noexcept
    {
        auto settings = winrt::make_self<implementation::MidiNetworkTransportSettings>();

        try
        {
            midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());
            command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_TRANSPORT_SETTINGS);

            auto response = midi2::ServiceConfig::MidiServiceTransportPluginConfigManager::SendCommand(command);

            if (response.Status() == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
            {
                auto responseJson = response.ResponseJson();

                if (responseJson != nullptr && responseJson.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_TRANSPORT_SETTINGS_RESPONSE_KEY))
                {
                    settings->InternalInitialize(
                        responseJson.GetNamedObject(MIDI_CONFIG_JSON_NETWORK_MIDI_TRANSPORT_SETTINGS_RESPONSE_KEY));
                }
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception reading the network transport settings.");
        }

        // An older service which does not know the command leaves the object on the defaults,
        // which is what that service is running with anyway.
        return *settings;
    }


    collections::IVectorView<network::MidiNetworkConfiguredHost> MidiNetworkTransportManager::GetConfiguredHosts() noexcept
    {        auto results = winrt::single_threaded_vector<network::MidiNetworkConfiguredHost>();

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
                            winrt::guid hostId{};

                            if (!TryParseGuid(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONFIG_ID_KEY, L""), hostId))
                            {
                                TraceLoggingWrite(
                                    Midi2SdkTelemetryProvider::Provider(),
                                    MIDI_SDK_TRACE_EVENT_ERROR,
                                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                                    TraceLoggingWideString(L"Host entry skipped. Its entry identifier is not a valid guid.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                                    TraceLoggingWideString(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONFIG_ID_KEY, L"").c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
                                );

                                continue;
                            }

                            auto host = winrt::make_self<MidiNetworkConfiguredHost>();

                            host->InternalInitialize(
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_IS_ENABLED_KEY, false),
                                hostId,
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NAME_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_PRODUCT_INSTANCE_ID_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_SERVICE_INSTANCE_NAME_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_SERVICE_INSTANCE_NAME_KEY, L""),
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_SERVICE_INSTANCE_NAME_CHANGED_KEY, false),
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HAS_STARTED_KEY, false),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_ADDRESS_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_PORT_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONFIGURED_PORT_KEY, L""),
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ALLOW_PORT_FALLBACK_KEY, true),
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_PORT_FALLBACK_USED_KEY, false),
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CREATE_MIDI1_PORTS_KEY, false),
                                RemoteClientPolicyFromString(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_REMOTE_CLIENT_POLICY_KEY, L""))
                            );

                            // remote clients on this host. The service reports an empty array when
                            // nothing has connected, so an older service simply yields no entries.
                            if (entryObject.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONNECTIONS_ARRAY_KEY))
                            {
                                auto connectionsArray = entryObject.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONNECTIONS_ARRAY_KEY);

                                for (auto const& connectionEntry : connectionsArray)
                                {
                                    auto connectionObject = connectionEntry.GetObject();

                                    if (connectionObject == nullptr)
                                    {
                                        continue;
                                    }

                                    auto connection = winrt::make_self<MidiNetworkHostConnection>();

                                    connection->InternalInitialize(
                                        connectionObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY, L""),
                                        connectionObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY, L""),
                                        connectionObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_REMOTE_ADDRESS_KEY, L""),
                                        connectionObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_REMOTE_PORT_KEY, L""),
                                        connectionObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_SESSION_ACTIVE_KEY, false),
                                        connectionObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_PENDING_APPROVAL_KEY, false),
                                        connectionObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_UMP_ENDPOINT_ID_KEY, L""),

                                        static_cast<uint64_t>(connectionObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_CURRENT_LATENCY_KEY, 0)),
                                        static_cast<uint32_t>(connectionObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_TOTAL_RETRANSMIT_COUNT_KEY, 0)),
                                        static_cast<uint32_t>(connectionObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_TOTAL_RETRANSMIT_REQUEST_COUNT_KEY, 0)),
                                        static_cast<uint64_t>(connectionObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_TOTAL_NETWORK_PACKETS_SENT_KEY, 0)),
                                        static_cast<uint64_t>(connectionObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_TOTAL_NETWORK_PACKETS_RECEIVED_KEY, 0)));

                                    host->InternalAddConnection(*connection);
                                }
                            }

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
                            winrt::guid clientId{};

                            if (!TryParseGuid(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CONFIG_ID_KEY, L""), clientId))
                            {
                                TraceLoggingWrite(
                                    Midi2SdkTelemetryProvider::Provider(),
                                    MIDI_SDK_TRACE_EVENT_ERROR,
                                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                                    TraceLoggingWideString(L"Client entry skipped. Its entry identifier is not a valid guid.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                                    TraceLoggingWideString(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CONFIG_ID_KEY, L"").c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
                                );

                                continue;
                            }

                            auto client = winrt::make_self<MidiNetworkConfiguredClient>();

                            client->InternalInitialize(
                                clientId,
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
                                static_cast<uint64_t>(entryObject.GetNamedNumber(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_NETWORK_PACKETS_RECEIVED_KEY, 0)),

                                EntryStateFromString(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_ENTRY_STATE_KEY, L"")),
                                entryObject.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_DIRECT_KEY, false),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_DIRECT_ADDRESS_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_DIRECT_PORT_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_MDNS_MATCH_ID_KEY, L"")
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


    namespace
    {
        // Inverse of the service's PendingRequestTimeToString. The wire format is ISO 8601 UTC
        // with the full 100ns FILETIME resolution, for example 2026-08-12T01:23:45.6789012Z. An
        // empty or unparseable string becomes a zero DateTime, which is what an unset request
        // time means on the service side as well.
        foundation::DateTime PendingRequestTimeFromString(_In_ winrt::hstring const& value) noexcept
        {
            foundation::DateTime result{};

            if (value.empty())
            {
                return result;
            }

            uint32_t year{}, month{}, day{}, hour{}, minute{}, second{}, fraction{};

            // The fraction is exactly seven digits, so it is read as a whole number of 100ns ticks
            if (swscanf_s(value.c_str(), L"%4u-%2u-%2uT%2u:%2u:%2u.%7uZ",
                &year, &month, &day, &hour, &minute, &second, &fraction) != 7)
            {
                return result;
            }

            SYSTEMTIME st{};
            st.wYear = static_cast<WORD>(year);
            st.wMonth = static_cast<WORD>(month);
            st.wDay = static_cast<WORD>(day);
            st.wHour = static_cast<WORD>(hour);
            st.wMinute = static_cast<WORD>(minute);
            st.wSecond = static_cast<WORD>(second);
            st.wMilliseconds = 0;

            FILETIME ft{};

            if (!SystemTimeToFileTime(&st, &ft))
            {
                return result;
            }

            // whole seconds from the conversion, plus the sub-second ticks the service preserved
            uint64_t fileTime = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
            fileTime += fraction;

            result = winrt::clock::from_file_time(winrt::file_time{ fileTime });

            return result;
        }
    }

    collections::IVectorView<network::MidiNetworkPendingRemoteClient> MidiNetworkTransportManager::GetPendingRemoteClients() noexcept
    {
        auto results = winrt::single_threaded_vector<network::MidiNetworkPendingRemoteClient>();

        try
        {
            midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());
            command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS);

            auto response = midi2::ServiceConfig::MidiServiceTransportPluginConfigManager::SendCommand(command);

            if (response.Status() == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
            {
                auto responseJson = response.ResponseJson();

                if (responseJson != nullptr && responseJson.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENTS_RESPONSE_ARRAY_KEY))
                {
                    auto pendingArray = responseJson.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENTS_RESPONSE_ARRAY_KEY);

                    for (auto const& entry : pendingArray)
                    {
                        auto entryObject = entry.GetObject();

                        if (entryObject != nullptr)
                        {
                            winrt::guid pendingHostId{};

                            if (!TryParseGuid(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER, L""), pendingHostId))
                            {
                                TraceLoggingWrite(
                                    Midi2SdkTelemetryProvider::Provider(),
                                    MIDI_SDK_TRACE_EVENT_ERROR,
                                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                                    TraceLoggingWideString(L"Pending client entry skipped. Its host entry identifier is not a valid guid.", MIDI_SDK_TRACE_MESSAGE_FIELD)
                                );

                                continue;
                            }

                            auto pendingClient = winrt::make_self<MidiNetworkPendingRemoteClient>();

                            pendingClient->InternalInitialize(
                                pendingHostId,
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENT_HOST_SERVICE_INSTANCE_NAME_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENT_HOST_NAME_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY, L""),
                                entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_REMOTE_ADDRESS_KEY, L""),
                                PendingRequestTimeFromString(entryObject.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENT_REQUEST_TIME_KEY, L""))
                            );

                            results.Append(*pendingClient);
                        }
                    }
                }
                else
                {
                    // no response array. Nothing pending is a normal and common answer.
                }
            }
            else
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Service returned a failure for the pending remote clients request.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(response.ServiceErrorMessage().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
                );
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
                TraceLoggingWideString(L"Unable to get pending remote clients. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
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
                TraceLoggingWideString(L"Unable to get pending remote clients. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return results.GetView();
    }






    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkHostCreationResponse> MidiNetworkTransportManager::CreateNetworkHostAsync(
        network::MidiNetworkHostCreationConfig const& creationConfig) noexcept
    {
        auto result = winrt::make_self<MidiNetworkHostCreationResponse>();

        try
        {
            if (creationConfig == nullptr)
            {
                result->InternalSetError(
                    network::MidiNetworkHostCreationErrorCode::InvalidArgument,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_NULL_CREATION_CONFIG));

                co_return *result;
            }

            // Strong copy, because a reference parameter is not stored in the coroutine frame
            // and does not survive the suspension below.
            auto config = creationConfig;

            co_await winrt::resume_background();

            auto createResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(config);

            if (createResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                // The update call only queues the definition. Returning here would hand back a
                // host the caller cannot yet use, so wait for the service to bring it up.
                bool started{ false };

                for (uint32_t attempt = 0; attempt < HostStartPollAttempts; attempt++)
                {
                    if (ConfiguredHostHasStarted(GetConfiguredHosts(), config.HostId()))
                    {
                        started = true;
                        break;
                    }

                    co_await winrt::resume_after(HostStartPollInterval);
                }

                if (started)
                {
                    result->InternalSetSuccess();
                }
                else
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Network host definition was accepted, but the host did not start in time.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingGuid(config.HostId(), "host id")
                    );

                    result->InternalSetError(
                        network::MidiNetworkHostCreationErrorCode::TimedOutWaitingForHostToStart,
                        internal::ResourceGetHString(IDS_NETWORK_ERROR_HOST_START_TIMEOUT));
                }
            }
            else
            {
                result->InternalSetError(
                    static_cast<network::MidiNetworkHostCreationErrorCode>(createResponse.ServiceErrorCode()), 
                    createResponse.ServiceErrorMessage());
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

            result->InternalSetError(network::MidiNetworkHostCreationErrorCode::ClientApiException, ex.message());

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

            result->InternalSetError(network::MidiNetworkHostCreationErrorCode::ClientApiException, internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

            co_return *result;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkHostRemovalResponse>
    MidiNetworkTransportManager::RemoveNetworkHostAsync(
        network::MidiNetworkHostRemovalConfig const& removalConfig) noexcept
    {
        auto result = winrt::make_self<MidiNetworkHostRemovalResponse>();

        try
        {
            if (removalConfig == nullptr)
            {
                result->InternalSetError(
                    network::MidiNetworkHostRemovalErrorCode::InvalidArgument,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_NULL_REMOVAL_CONFIG));

                co_return *result;
            }

            auto const hostId = removalConfig.HostId();

            result->InternalSetHostId(hostId);

            midi2::ServiceConfig::MidiServiceTransportCommand command(network::MidiNetworkTransportManager::TransportId());

            command.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_REMOVE_HOST);
            command.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                winrt::to_hstring(hostId));

            co_await winrt::resume_background();

            // the service shuts the host down synchronously, including its sessions, so this can take a moment
            auto response = midi2::ServiceConfig::MidiServiceTransportPluginConfigManager::SendCommand(command);

            if (response.Status() == midi2::ServiceConfig::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Service rejected the host removal.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(response.ServiceErrorMessage().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
                );

                result->InternalSetError(
                    static_cast<network::MidiNetworkHostRemovalErrorCode>(response.ServiceErrorCode()),
                    response.ServiceErrorMessage());
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
                TraceLoggingWideString(L"Unable to remove network host. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            result->InternalSetError(network::MidiNetworkHostRemovalErrorCode::ClientApiException, ex.message());

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
                TraceLoggingWideString(L"Unable to remove network host. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            result->InternalSetError(network::MidiNetworkHostRemovalErrorCode::ClientApiException, internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

            co_return *result;
        }
    }



    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkClientConnectResponse>
    MidiNetworkTransportManager::ConnectNetworkClientAsync(
        network::MidiNetworkClientConnectConfig const& creationConfig) noexcept
    {
        auto result = winrt::make_self<MidiNetworkClientConnectResponse>();

        try
        {
            if (creationConfig == nullptr)
            {
                result->InternalSetError(
                    network::MidiNetworkClientConnectErrorCode::InvalidArgument,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_NULL_CONNECT_CONFIG));

                co_return *result;
            }

            auto matchCriteria = creationConfig.MatchCriteria();

            if (matchCriteria == nullptr)
            {
                result->InternalSetError(
                    network::MidiNetworkClientConnectErrorCode::InvalidOrMissingMatchCriteria,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_MISSING_MATCH_CRITERIA));

                co_return *result;
            }

            svc::MidiServiceTransportCommand cmd(MidiNetworkTransportManager::TransportId());

            auto const deviceId = matchCriteria.DeviceId();

            if (!deviceId.empty())
            {
                // A discovered host is matched by device id, and the service re-resolves its
                // address from the advertisement every time it appears. That is what lets the
                // connection survive the remote moving to a new address or port, which a direct
                // connection cannot do.
                cmd.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_MDNS);
                cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER, winrt::to_hstring(creationConfig.ClientId()));
                cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_MATCH_ID, deviceId);
                cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME, creationConfig.UmpEndpointName());
            }
            else if (!matchCriteria.DirectHostNameOrIPAddress().empty())
            {
                cmd.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_DIRECT);
                cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER, winrt::to_hstring(creationConfig.ClientId()));
                cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_ADDRESS, matchCriteria.DirectHostNameOrIPAddress());
                cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_PORT, winrt::to_hstring(matchCriteria.DirectPort()));
                cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME, creationConfig.UmpEndpointName());
            }
            else
            {
                result->InternalSetError(
                    network::MidiNetworkClientConnectErrorCode::InvalidOrMissingMatchCriteria,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_MISSING_MATCH_CRITERIA));

                co_return *result;
            }

            // Optional. The service applies this before it activates the endpoint, so a named
            // connection is never created under the remote's name and renamed a moment later.
            if (!creationConfig.CustomEndpointName().empty())
            {
                cmd.Arguments().Insert(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_CUSTOM_ENDPOINT_NAME_KEY,
                    creationConfig.CustomEndpointName());
            }

            // Decides whether the endpoint is built with MIDI 1.0 ports, so it has to reach the
            // service with the creation rather than afterwards.
            cmd.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY,
                creationConfig.CreateOnlyUmpEndpoints() ? L"false" : L"true");

            co_await winrt::resume_background();

            // the service sends the invitation and waits for the reply, so this can take a while
            auto createResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

            if (createResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                result->InternalSetError(static_cast<network::MidiNetworkClientConnectErrorCode>(createResponse.ServiceErrorCode()), createResponse.ServiceErrorMessage());
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
                TraceLoggingWideString(L"Unable to connect network client. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            result->InternalSetError(network::MidiNetworkClientConnectErrorCode::ClientApiException, ex.message());

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
                TraceLoggingWideString(L"Unable to connect network client. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            result->InternalSetError(network::MidiNetworkClientConnectErrorCode::ClientApiException, internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

            co_return *result;
        }
    }
    

    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkClientDisconnectResponse>
    MidiNetworkTransportManager::DisconnectNetworkClientAsync(
        network::MidiNetworkClientDisconnectConfig const& disconnectConfig) noexcept
    {
        auto result = winrt::make_self<MidiNetworkClientDisconnectResponse>();

        try
        {
            if (disconnectConfig == nullptr)
            {
                result->InternalSetError(
                    network::MidiNetworkClientDisconnectErrorCode::InvalidArgument,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_NULL_DISCONNECT_CONFIG));

                co_return *result;
            }

            svc::MidiServiceTransportCommand cmd(MidiNetworkTransportManager::TransportId());
            cmd.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT);
            cmd.Arguments().Insert(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER, winrt::to_hstring(disconnectConfig.ClientId()));

            co_await winrt::resume_background();

            auto response = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

            if (response.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                result->InternalSetSuccess();
            }
            else
            {
                // TODO: Get actual error code
                result->InternalSetError(static_cast<network::MidiNetworkClientDisconnectErrorCode>(response.ServiceErrorCode()), response.ServiceErrorMessage());
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
            result->InternalSetError(network::MidiNetworkClientDisconnectErrorCode::ClientApiException, ex.message());

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
            result->InternalSetError(network::MidiNetworkClientDisconnectErrorCode::ClientApiException, internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

            co_return *result;
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkRemoteClientApprovalResponse> MidiNetworkTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(
        network::MidiNetworkRemoteClientApprovalConfig const& approvalConfig) noexcept
    {
        auto response = winrt::make_self<MidiNetworkRemoteClientApprovalResponse>();

        try
        {
            if (approvalConfig == nullptr)
            {
                response->InternalSetError(
                    network::MidiNetworkRemoteClientApprovalErrorCode::InvalidArgument,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_NULL_APPROVAL_CONFIG));

                co_return *response;
            }

            // Captured before the thread switch because the projected object may be apartment-bound
            auto const hostId = approvalConfig.HostId();
            auto const remoteClientName = approvalConfig.RemoteClientName();
            auto const remoteClientProductInstanceId = approvalConfig.RemoteClientProductInstanceId();
            auto const approve = approvalConfig.Approve();
            auto const scopeIsThisRequestOnly = approvalConfig.ScopeIsThisRequestOnly();

            response->InternalSetHostId(hostId);
            response->InternalSetRemoteClientName(remoteClientName);
            response->InternalSetRemoteClientProductInstanceId(remoteClientProductInstanceId);

            if (remoteClientName.empty() || remoteClientProductInstanceId.empty())
            {
                response->InternalSetError(
                    network::MidiNetworkRemoteClientApprovalErrorCode::InvalidOrMissingRemoteClientIdentity,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_MISSING_REMOTE_CLIENT_IDENTITY));

                co_return *response;
            }

            svc::MidiServiceTransportCommand cmd(MidiNetworkTransportManager::TransportId());

            cmd.Verb(approve ?
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT :
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT);

            cmd.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                winrt::to_hstring(hostId));

            // The service matches the parked connection on these two values together
            cmd.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                remoteClientName);

            cmd.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY,
                remoteClientProductInstanceId);

            // Only "always" is written to the configuration file. "once" authorizes just the
            // connection currently waiting.
            cmd.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_APPROVAL_SCOPE,
                scopeIsThisRequestOnly ?
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_APPROVAL_SCOPE_ONCE :
                    MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_APPROVAL_SCOPE_ALWAYS);

            co_await resume_background();

            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

            if (serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                response->InternalSetSuccess();
            }
            else
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Service rejected the remote client decision.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(serviceResponse.ServiceErrorMessage().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
                );

                response->InternalSetError(
                    static_cast<network::MidiNetworkRemoteClientApprovalErrorCode>(serviceResponse.ServiceErrorCode()),
                    serviceResponse.ServiceErrorMessage());
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
                TraceLoggingWideString(L"Unable to approve or deny remote client. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            response->InternalSetError(network::MidiNetworkRemoteClientApprovalErrorCode::ClientApiException, ex.message());

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
                TraceLoggingWideString(L"Unable to approve or deny remote client. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            response->InternalSetError(network::MidiNetworkRemoteClientApprovalErrorCode::ClientApiException, internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

            co_return *response;
        }
    }


    _Use_decl_annotations_
    foundation::IAsyncOperation<network::MidiNetworkRemoteClientDisconnectResponse> MidiNetworkTransportManager::DisconnectRemoteClientAsync(
        network::MidiNetworkRemoteClientDisconnectConfig const& disconnectConfig) noexcept
    {
        auto response = winrt::make_self<MidiNetworkRemoteClientDisconnectResponse>();

        try
        {
            if (disconnectConfig == nullptr)
            {
                response->InternalSetError(
                    network::MidiNetworkRemoteClientDisconnectErrorCode::InvalidArgument,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_NULL_DISCONNECT_CONFIG));

                co_return *response;
            }

            // Captured before the thread switch because the projected object may be apartment-bound
            auto const hostId = disconnectConfig.HostId();
            auto const remoteClientName = disconnectConfig.RemoteClientName();
            auto const remoteClientProductInstanceId = disconnectConfig.RemoteClientProductInstanceId();

            response->InternalSetHostId(hostId);
            response->InternalSetRemoteClientName(remoteClientName);
            response->InternalSetRemoteClientProductInstanceId(remoteClientProductInstanceId);

            if (remoteClientName.empty() || remoteClientProductInstanceId.empty())
            {
                response->InternalSetError(
                    network::MidiNetworkRemoteClientDisconnectErrorCode::InvalidOrMissingRemoteClientIdentity,
                    internal::ResourceGetHString(IDS_NETWORK_ERROR_MISSING_REMOTE_CLIENT_IDENTITY));

                co_return *response;
            }

            svc::MidiServiceTransportCommand cmd(MidiNetworkTransportManager::TransportId());

            cmd.Verb(MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_REMOTE_CLIENT);

            cmd.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER,
                winrt::to_hstring(hostId));

            cmd.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                remoteClientName);

            cmd.Arguments().Insert(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY,
                remoteClientProductInstanceId);

            co_await resume_background();

            auto serviceResponse = svc::MidiServiceTransportPluginConfigManager::SendCommand(cmd);

            if (serviceResponse.Status() == svc::MidiServiceConfigResponseStatus::Success)
            {
                response->InternalSetSuccess();
            }
            else
            {
                response->InternalSetError(
                    static_cast<network::MidiNetworkRemoteClientDisconnectErrorCode>(serviceResponse.ServiceErrorCode()),
                    serviceResponse.ServiceErrorMessage());
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
                TraceLoggingWideString(L"Unable to disconnect remote client. HRESULT exception.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(ex.code(), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            response->InternalSetError(network::MidiNetworkRemoteClientDisconnectErrorCode::ClientApiException, ex.message());

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
                TraceLoggingWideString(L"Unable to disconnect remote client. General exception.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            response->InternalSetError(network::MidiNetworkRemoteClientDisconnectErrorCode::ClientApiException, internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION));

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

    winrt::hstring MidiNetworkTransportManager::MidiNetworkUdpDnsSdQueryName() noexcept
    {
        return MidiNetworkUdpDnsServiceType() + L"." + MidiNetworkUdpDnsDomain();
    }


    uint16_t MidiNetworkTransportManager::GenerateAvailableHostPort() noexcept
    {
        uint16_t port{ 0 };

        if (!::WindowsMidiServicesInternal::TryGenerateAvailableHostPort(port))
        {
            return 0;
        }

        return port;
    }

    _Use_decl_annotations_
    bool MidiNetworkTransportManager::IsHostPortAvailable(uint16_t const port) noexcept
    {
        return ::WindowsMidiServicesInternal::IsUdpPortAvailable(port);
    }


    // A DNS-SD responder answers a fresh query almost immediately, so this browses for a short
    // settling period and returns what replied. The previous implementation went through
    // Windows.Devices.Enumeration and took a fixed thirty seconds every time.
    collections::IVectorView<network::MidiNetworkAdvertisedHost> MidiNetworkTransportManager::GetAdvertisedHosts() noexcept
    {
        auto results = winrt::single_threaded_vector<network::MidiNetworkAdvertisedHost>();

        try
        {
            ::WindowsMidiServicesInternal::MidiDnssdBrowser browser;

            auto const hr = browser.Start(
                std::wstring{ MidiNetworkUdpDnsSdQueryName() },
                nullptr,
                nullptr,
                nullptr);

            if (FAILED(hr))
            {
                LOG_IF_FAILED(hr);

                return results.GetView();
            }

            auto stopBrowser = wil::scope_exit([&browser]() { browser.Stop(); });

            std::this_thread::sleep_for(std::chrono::seconds(2));

            for (auto const& service : browser.EnumeratedServices())
            {
                auto host = winrt::make_self<network::implementation::MidiNetworkAdvertisedHost>();

                host->InternalInitializeFromDnssdService(service);

                results.Append(*host);
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
