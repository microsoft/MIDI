// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpTransportManager.h"
#include "Transports.Rtp.MidiRtpTransportManager.g.cpp"

#include "MidiReporting.h"
#include "MidiServiceTransportPluginConfigManager.h"

#include "MidiRtpHostCreationResponse.h"
#include "MidiRtpHostRemovalResponse.h"
#include "MidiRtpHostUpdateResponse.h"
#include "MidiRtpClientConnectResponse.h"
#include "MidiRtpClientDisconnectResponse.h"
#include "MidiRtpRemoteClientApprovalResponse.h"
#include "MidiRtpRemoteClientDisconnectResponse.h"
#include "MidiRtpRemoteClientForgetResponse.h"

#include "MidiRtpConfiguredHost.h"
#include "MidiRtpConfiguredClient.h"
#include "MidiRtpPendingRemoteClient.h"
#include "MidiRtpAdvertisedHost.h"

#include "MidiRtpSavedHost.h"
#include "MidiRtpSavedClient.h"

#include "MidiConfigFile.h"
#include "midi_saved_config_json.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    namespace
    {
        // The service starts a host on its own worker thread, so the host is not up yet when the
        // configuration update returns
        constexpr uint32_t HostStartPollAttempts{ 40 };
        constexpr std::chrono::milliseconds HostStartPollInterval{ 250 };

        using CommandArguments = std::vector<std::pair<winrt::hstring, winrt::hstring>>;

        // for the catch handlers below, which must not throw
        winrt::hstring GeneralExceptionMessage() noexcept
        {
            try
            {
                return internal::ResourceGetHString(IDS_ERROR_GENERAL_EXCEPTION);
            }
            catch (...)
            {
                return {};
            }
        }

        // A request which never reached the transport has no transport error code, because the
        // service is not running or the transport is not installed
        template <typename TErrorCode, typename TResult>
        void ApplyServiceResponse(_In_ svc::MidiServiceConfigResponse const& response, _Inout_ TResult& result) noexcept
        {
            try
            {
                if (response != nullptr && response.Status() == svc::MidiServiceConfigResponseStatus::Success)
                {
                    result.InternalSetSuccess();
                    return;
                }

                if (response != nullptr &&
                    response.Status() == svc::MidiServiceConfigResponseStatus::ErrorFromService &&
                    response.ServiceErrorCode() != RTP_MIDI_ERROR_CODE_UNKNOWN_ERROR)
                {
                    result.InternalSetError(static_cast<TErrorCode>(response.ServiceErrorCode()), response.ServiceErrorMessage());
                    return;
                }

                auto message = response == nullptr ? winrt::hstring{} : response.ServiceErrorMessage();
                if (message.empty()) message = internal::ResourceGetHString(IDS_RTP_ERROR_SERVICE_UNAVAILABLE);

                result.InternalSetError(static_cast<TErrorCode>(RTP_MIDI_ERROR_CODE_CLIENT_API_SERVICE_UNAVAILABLE), message);
            }
            catch (...)
            {
                result.InternalSetError(static_cast<TErrorCode>(RTP_MIDI_ERROR_CODE_CLIENT_API_EXCEPTION), winrt::hstring{});
            }
        }

        svc::MidiServiceConfigResponse SendCommand(_In_ wchar_t const* const verb, _In_ CommandArguments const& arguments)
        {
            svc::MidiServiceTransportCommand command(MidiRtpTransportManager::TransportId());
            command.Verb(verb);

            for (auto const& argument : arguments) command.Arguments().Insert(argument.first, argument.second);

            return svc::MidiServiceTransportPluginConfigManager::SendCommand(command);
        }

        template <typename TErrorCode, typename TResult>
        void RunCommand(_In_ wchar_t const* const verb, _In_ CommandArguments const& arguments, _Inout_ TResult& result) noexcept
        {
            try
            {
                ApplyServiceResponse<TErrorCode>(SendCommand(verb, arguments), result);
            }
            catch (winrt::hresult_error const& ex)
            {
                MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error sending an RTP-MIDI command.");
                result.InternalSetError(static_cast<TErrorCode>(RTP_MIDI_ERROR_CODE_CLIENT_API_EXCEPTION), ex.message());
            }
            catch (...)
            {
                MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception sending an RTP-MIDI command.");
                result.InternalSetError(static_cast<TErrorCode>(RTP_MIDI_ERROR_CODE_CLIENT_API_EXCEPTION), GeneralExceptionMessage());
            }
        }

        template <typename TErrorCode, typename TResult>
        void RunUpdate(_In_ json::JsonObject const& configJson, _Inout_ TResult& result) noexcept
        {
            try
            {
                ApplyServiceResponse<TErrorCode>(
                    svc::MidiServiceTransportPluginConfigManager::SendUpdate(MidiRtpTransportManager::TransportId(), configJson),
                    result);
            }
            catch (winrt::hresult_error const& ex)
            {
                MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error sending an RTP-MIDI configuration update.");
                result.InternalSetError(static_cast<TErrorCode>(RTP_MIDI_ERROR_CODE_CLIENT_API_EXCEPTION), ex.message());
            }
            catch (...)
            {
                MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception sending an RTP-MIDI configuration update.");
                result.InternalSetError(static_cast<TErrorCode>(RTP_MIDI_ERROR_CODE_CLIENT_API_EXCEPTION), GeneralExceptionMessage());
            }
        }

        std::pair<winrt::hstring, winrt::hstring> EntryArgument(_In_ winrt::guid const& entryId)
        {
            return { MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_ENTRY_IDENTIFIER, MidiRtpSdkJson::EntryKey(entryId) };
        }

        // the answer to a query, or nullptr when there is none to read
        json::JsonObject Query(_In_ wchar_t const* const verb) noexcept
        {
            try
            {
                auto const response = SendCommand(verb, {});

                if (response == nullptr || response.Status() != svc::MidiServiceConfigResponseStatus::Success) return nullptr;

                return response.ResponseJson();
            }
            catch (...)
            {
                MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception querying the RTP-MIDI transport.");
                return nullptr;
            }
        }

        bool HostHasStarted(_In_ winrt::guid const& hostId) noexcept
        {
            try
            {
                for (auto const& host : MidiRtpTransportManager::GetConfiguredHosts())
                {
                    if (host.HostId() == hostId) return host.HasStarted();
                }
            }
            catch (...)
            {
            }

            return false;
        }
    }


    bool MidiRtpTransportManager::IsTransportAvailable() noexcept
    {
        try
        {
            for (auto const& transport : rpt::MidiReporting::GetInstalledTransportPlugins())
            {
                if (transport.TransportId() == TransportId()) return true;
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception listing the installed transports.");
        }

        return false;
    }


    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpHostCreationResponse> MidiRtpTransportManager::CreateRtpHostAsync(rtp::MidiRtpHostCreationConfig const creationConfig) noexcept
    {
        auto result = winrt::make_self<MidiRtpHostCreationResponse>();

        if (creationConfig == nullptr)
        {
            result->InternalSetError(rtp::MidiRtpHostCreationErrorCode::InvalidArgument, internal::ResourceGetHString(IDS_RTP_ERROR_NULL_CONFIG));
            co_return *result;
        }

        auto const hostId = creationConfig.HostId();
        auto const configJson = creationConfig.ConfigJson();

        result->InternalSetHostId(hostId);

        co_await winrt::resume_background();

        RunUpdate<rtp::MidiRtpHostCreationErrorCode>(configJson, *result);

        if (!result->Success()) co_return *result;

        // Returning now would hand back a host the caller cannot use yet
        for (uint32_t attempt = 0; attempt < HostStartPollAttempts; attempt++)
        {
            if (HostHasStarted(hostId)) co_return *result;

            co_await winrt::resume_after(HostStartPollInterval);
        }

        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingWideString(L"RTP-MIDI host was accepted, but did not start in time.", MIDI_SDK_TRACE_MESSAGE_FIELD),
            TraceLoggingGuid(hostId, "host id")
        );

        result->InternalSetError(rtp::MidiRtpHostCreationErrorCode::TimedOutWaitingForHostToStart, internal::ResourceGetHString(IDS_RTP_ERROR_HOST_START_TIMEOUT));

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpHostRemovalResponse> MidiRtpTransportManager::RemoveRtpHostAsync(rtp::MidiRtpHostRemovalConfig const removalConfig) noexcept
    {
        auto result = winrt::make_self<MidiRtpHostRemovalResponse>();

        if (removalConfig == nullptr)
        {
            result->InternalSetError(rtp::MidiRtpHostRemovalErrorCode::InvalidArgument, internal::ResourceGetHString(IDS_RTP_ERROR_NULL_CONFIG));
            co_return *result;
        }

        auto const hostId = removalConfig.HostId();
        result->InternalSetHostId(hostId);

        co_await winrt::resume_background();

        // a command rather than the removal config, because the command also stops the host
        RunCommand<rtp::MidiRtpHostRemovalErrorCode>(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_HOST, { EntryArgument(hostId) }, *result);

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpHostUpdateResponse> MidiRtpTransportManager::StopRtpHostAsync(winrt::guid const hostId) noexcept
    {
        auto result = winrt::make_self<MidiRtpHostUpdateResponse>();
        result->InternalSetHostId(hostId);

        co_await winrt::resume_background();

        RunCommand<rtp::MidiRtpHostUpdateErrorCode>(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_STOP_HOST, { EntryArgument(hostId) }, *result);

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpHostUpdateResponse> MidiRtpTransportManager::StartRtpHostAsync(winrt::guid const hostId) noexcept
    {
        auto result = winrt::make_self<MidiRtpHostUpdateResponse>();
        result->InternalSetHostId(hostId);

        co_await winrt::resume_background();

        RunCommand<rtp::MidiRtpHostUpdateErrorCode>(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_START_HOST, { EntryArgument(hostId) }, *result);

        co_return *result;
    }


    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpClientConnectResponse> MidiRtpTransportManager::ConnectRtpClientAsync(rtp::MidiRtpClientConnectConfig const connectConfig) noexcept
    {
        auto result = winrt::make_self<MidiRtpClientConnectResponse>();

        if (connectConfig == nullptr)
        {
            result->InternalSetError(rtp::MidiRtpClientConnectErrorCode::InvalidArgument, internal::ResourceGetHString(IDS_RTP_ERROR_NULL_CONFIG));
            co_return *result;
        }

        auto const configJson = connectConfig.ConfigJson();
        result->InternalSetClientId(connectConfig.ClientId());

        co_await winrt::resume_background();

        // the service checks the match criteria, so there is one set of rules for the file and the API
        RunUpdate<rtp::MidiRtpClientConnectErrorCode>(configJson, *result);

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpClientConnectResponse> MidiRtpTransportManager::ReconnectRtpClientAsync(winrt::guid const clientId) noexcept
    {
        auto result = winrt::make_self<MidiRtpClientConnectResponse>();
        result->InternalSetClientId(clientId);

        co_await winrt::resume_background();

        RunCommand<rtp::MidiRtpClientConnectErrorCode>(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_RECONNECT_CLIENT, { EntryArgument(clientId) }, *result);

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpClientDisconnectResponse> MidiRtpTransportManager::DisconnectRtpClientAsync(rtp::MidiRtpClientDisconnectConfig const disconnectConfig) noexcept
    {
        auto result = winrt::make_self<MidiRtpClientDisconnectResponse>();

        if (disconnectConfig == nullptr)
        {
            result->InternalSetError(rtp::MidiRtpClientDisconnectErrorCode::InvalidArgument, internal::ResourceGetHString(IDS_RTP_ERROR_NULL_CONFIG));
            co_return *result;
        }

        auto const clientId = disconnectConfig.ClientId();
        result->InternalSetClientId(clientId);

        co_await winrt::resume_background();

        RunCommand<rtp::MidiRtpClientDisconnectErrorCode>(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_CLIENT, { EntryArgument(clientId) }, *result);

        co_return *result;
    }


    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpRemoteClientApprovalResponse> MidiRtpTransportManager::ApproveOrDenyRemoteClientConnectRequestAsync(rtp::MidiRtpRemoteClientApprovalConfig const approvalConfig) noexcept
    {
        auto result = winrt::make_self<MidiRtpRemoteClientApprovalResponse>();

        if (approvalConfig == nullptr)
        {
            result->InternalSetError(rtp::MidiRtpRemoteClientApprovalErrorCode::InvalidArgument, internal::ResourceGetHString(IDS_RTP_ERROR_NULL_CONFIG));
            co_return *result;
        }

        auto const hostId = approvalConfig.HostId();
        auto const remoteClientName = approvalConfig.RemoteClientName();
        auto const approve = approvalConfig.Approve();
        auto const thisRequestOnly = approvalConfig.ScopeIsThisRequestOnly();

        result->InternalSetHostId(hostId);
        result->InternalSetRemoteClientName(remoteClientName);

        co_await winrt::resume_background();

        // The service keeps an "always" decision until it restarts. Saving it to the file is the
        // caller's choice, through MidiRtpHostKnownClientsConfig.
        RunCommand<rtp::MidiRtpRemoteClientApprovalErrorCode>(
            approve ? MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT : MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT,
            {
                EntryArgument(hostId),
                { MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_REMOTE_NAME, remoteClientName },
                { MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_APPROVAL_SCOPE, thisRequestOnly ? MIDI_CONFIG_JSON_RTP_MIDI_APPROVAL_SCOPE_ONCE : MIDI_CONFIG_JSON_RTP_MIDI_APPROVAL_SCOPE_ALWAYS }
            },
            *result);

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpRemoteClientDisconnectResponse> MidiRtpTransportManager::DisconnectRemoteClientAsync(rtp::MidiRtpRemoteClientDisconnectConfig const disconnectConfig) noexcept
    {
        auto result = winrt::make_self<MidiRtpRemoteClientDisconnectResponse>();

        if (disconnectConfig == nullptr)
        {
            result->InternalSetError(rtp::MidiRtpRemoteClientDisconnectErrorCode::InvalidArgument, internal::ResourceGetHString(IDS_RTP_ERROR_NULL_CONFIG));
            co_return *result;
        }

        auto const hostId = disconnectConfig.HostId();
        auto const connectionId = disconnectConfig.ConnectionId();

        result->InternalSetHostId(hostId);

        co_await winrt::resume_background();

        RunCommand<rtp::MidiRtpRemoteClientDisconnectErrorCode>(
            MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DISCONNECT_REMOTE,
            {
                EntryArgument(hostId),
                { MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_CONNECTION_ID, winrt::to_hstring(connectionId) }
            },
            *result);

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpRemoteClientForgetResponse> MidiRtpTransportManager::ForgetRemoteClientAsync(rtp::MidiRtpRemoteClientForgetConfig const forgetConfig) noexcept
    {
        auto result = winrt::make_self<MidiRtpRemoteClientForgetResponse>();

        if (forgetConfig == nullptr)
        {
            result->InternalSetError(rtp::MidiRtpRemoteClientForgetErrorCode::InvalidArgument, internal::ResourceGetHString(IDS_RTP_ERROR_NULL_CONFIG));
            co_return *result;
        }

        auto const hostId = forgetConfig.HostId();
        auto const remoteClientName = forgetConfig.RemoteClientName();

        result->InternalSetHostId(hostId);
        result->InternalSetRemoteClientName(remoteClientName);

        co_await winrt::resume_background();

        RunCommand<rtp::MidiRtpRemoteClientForgetErrorCode>(
            MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_FORGET_REMOTE_CLIENT,
            {
                EntryArgument(hostId),
                { MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_REMOTE_NAME, remoteClientName }
            },
            *result);

        co_return *result;
    }


    // In each list, an entry the SDK cannot read costs that entry, not the whole list

    collections::IVectorView<rtp::MidiRtpSavedHost> MidiRtpTransportManager::GetSavedHosts() noexcept
    {
        auto results = winrt::single_threaded_vector<rtp::MidiRtpSavedHost>();

        try
        {
            auto const create = MidiSavedConfigJson::Object(
                svc::implementation::MidiConfigFile::LoadTransportSection(TransportId()),
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY);

            // Kept beside the hosts rather than inside them, so a decision can be saved without
            // rewriting the host
            auto const decisions = MidiSavedConfigJson::Entries(
                MidiSavedConfigJson::Object(create, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_DECISIONS_KEY));

            for (auto const& [hostId, entry] : MidiSavedConfigJson::Entries(
                MidiSavedConfigJson::Object(create, MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY)))
            {
                auto host = winrt::make_self<MidiRtpSavedHost>();

                host->InternalInitialize(hostId, entry, MidiSavedConfigJson::EntriesFor(decisions, hostId));

                results.Append(*host);
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception reading the saved RTP-MIDI hosts.");
        }

        return results.GetView();
    }

    collections::IVectorView<rtp::MidiRtpSavedClient> MidiRtpTransportManager::GetSavedClients() noexcept
    {
        auto results = winrt::single_threaded_vector<rtp::MidiRtpSavedClient>();

        try
        {
            auto const clients = MidiSavedConfigJson::Object(
                MidiSavedConfigJson::Object(
                    svc::implementation::MidiConfigFile::LoadTransportSection(TransportId()),
                    MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY),
                MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY);

            for (auto const& [clientId, entry] : MidiSavedConfigJson::Entries(clients))
            {
                auto client = winrt::make_self<MidiRtpSavedClient>();

                client->InternalInitialize(clientId, entry);

                results.Append(*client);
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception reading the saved RTP-MIDI clients.");
        }

        return results.GetView();
    }

    collections::IVectorView<rtp::MidiRtpConfiguredHost> MidiRtpTransportManager::GetConfiguredHosts() noexcept
    {
        auto results = winrt::single_threaded_vector<rtp::MidiRtpConfiguredHost>();

        try
        {
            for (auto const& entry : MidiRtpSdkJson::Objects(MidiRtpSdkJson::Array(Query(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_HOSTS), MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY)))
            {
                auto host = winrt::make_self<MidiRtpConfiguredHost>();
                if (host->InternalInitialize(entry)) results.Append(*host);
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception reading the RTP-MIDI hosts.");
        }

        return results.GetView();
    }

    collections::IVectorView<rtp::MidiRtpConfiguredClient> MidiRtpTransportManager::GetConfiguredClients() noexcept
    {
        auto results = winrt::single_threaded_vector<rtp::MidiRtpConfiguredClient>();

        try
        {
            for (auto const& entry : MidiRtpSdkJson::Objects(MidiRtpSdkJson::Array(Query(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS), MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY)))
            {
                auto client = winrt::make_self<MidiRtpConfiguredClient>();
                if (client->InternalInitialize(entry)) results.Append(*client);
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception reading the RTP-MIDI clients.");
        }

        return results.GetView();
    }

    collections::IVectorView<rtp::MidiRtpPendingRemoteClient> MidiRtpTransportManager::GetPendingRemoteClients() noexcept
    {
        auto results = winrt::single_threaded_vector<rtp::MidiRtpPendingRemoteClient>();

        try
        {
            for (auto const& entry : MidiRtpSdkJson::Objects(MidiRtpSdkJson::Array(Query(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS), MIDI_CONFIG_JSON_RTP_MIDI_PENDING_REMOTE_CLIENTS_KEY)))
            {
                auto pending = winrt::make_self<MidiRtpPendingRemoteClient>();
                if (pending->InternalInitialize(entry)) results.Append(*pending);
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception reading the RTP-MIDI pending remote clients.");
        }

        return results.GetView();
    }

    // The service browses all the time, so this answers at once rather than after a browse period
    collections::IVectorView<rtp::MidiRtpAdvertisedHost> MidiRtpTransportManager::GetAdvertisedHosts() noexcept
    {
        auto results = winrt::single_threaded_vector<rtp::MidiRtpAdvertisedHost>();

        try
        {
            for (auto const& entry : MidiRtpSdkJson::Objects(MidiRtpSdkJson::Array(Query(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_ADVERTISED), MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISED_HOSTS_KEY)))
            {
                auto host = winrt::make_self<MidiRtpAdvertisedHost>();
                host->InternalInitialize(entry);
                results.Append(*host);
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"Exception reading the advertised RTP-MIDI hosts.");
        }

        return results.GetView();
    }
}
