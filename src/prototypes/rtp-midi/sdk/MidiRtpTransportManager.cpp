// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Talks to the rtpMIDI transport in the MIDI service.
// ============================================================================

#include "pch.h"
#include "MidiRtpTransportManager.h"
#include "MidiRtpTransportManager.g.cpp"

#include "MidiRtpOperationResponse.h"
#include "MidiRtpConfiguredHost.h"
#include "MidiRtpConfiguredClient.h"
#include "MidiRtpAdvertisedPeer.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    namespace
    {
        // The service starts a host on its own worker thread, so it is not up yet when the
        // configuration update returns
        constexpr uint32_t HostStartPollAttempts{ 40 };
        constexpr std::chrono::milliseconds HostStartPollInterval{ 250 };

        void ApplyServiceResponse(_In_ svc::MidiServiceConfigResponse const& response, _Inout_ MidiRtpOperationResponse& result) noexcept
        {
            try
            {
                if (response != nullptr && response.Status() == svc::MidiServiceConfigResponseStatus::Success)
                {
                    result.InternalSetSuccess();
                    return;
                }

                auto const code = response == nullptr ? 0u : response.ServiceErrorCode();
                auto message = response == nullptr ? winrt::hstring{} : response.ServiceErrorMessage();

                // No transport error code means the request never reached the transport
                if (code == 0)
                {
                    result.InternalSetError(rtp::MidiRtpErrorCode::ServiceUnavailable,
                        message.empty() ? internal::ResourceGetHString(IDS_RTP_SDK_ERROR_SERVICE_UNAVAILABLE) : message);
                    return;
                }

                result.InternalSetError(static_cast<rtp::MidiRtpErrorCode>(code), message);
            }
            catch (...)
            {
                result.InternalSetError(rtp::MidiRtpErrorCode::ClientApiException, internal::ResourceGetHString(IDS_RTP_SDK_ERROR_EXCEPTION));
            }
        }

        svc::MidiServiceConfigResponse SendCommand(
            _In_ wchar_t const* const verb,
            _In_ std::vector<std::pair<winrt::hstring, winrt::hstring>> const& arguments)
        {
            svc::MidiServiceTransportCommand command(MIDI_RTP_TRANSPORT_ID_FOR_SDK);
            command.Verb(verb);

            for (auto const& argument : arguments) command.Arguments().Insert(argument.first, argument.second);

            return svc::MidiServiceTransportPluginConfigManager::SendCommand(command);
        }

        rtp::MidiRtpOperationResponse RunCommand(
            _In_ wchar_t const* const verb,
            _In_ std::vector<std::pair<winrt::hstring, winrt::hstring>> const& arguments,
            _In_ winrt::guid const& entryId) noexcept
        {
            auto result = winrt::make_self<MidiRtpOperationResponse>();
            result->InternalSetEntryId(entryId);

            try
            {
                ApplyServiceResponse(SendCommand(verb, arguments), *result);
            }
            catch (...)
            {
                result->InternalSetError(rtp::MidiRtpErrorCode::ClientApiException, internal::ResourceGetHString(IDS_RTP_SDK_ERROR_EXCEPTION));
            }

            return *result;
        }

        std::pair<winrt::hstring, winrt::hstring> EntryArgument(_In_ winrt::guid const& entryId)
        {
            return { MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_ENTRY_IDENTIFIER, winrt::to_hstring(entryId) };
        }

        // The answer to a query, or nullptr when there is none to read
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
                return nullptr;
            }
        }
    }


    bool MidiRtpTransportManager::IsTransportAvailable() noexcept
    {
        try
        {
            for (auto const& transport : rpt::MidiReporting::GetInstalledTransportPlugins())
            {
                if (transport != nullptr && transport.TransportId() == TransportId()) return true;
            }
        }
        catch (...)
        {
        }

        return false;
    }


    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> MidiRtpTransportManager::CreateHostAsync(rtp::MidiRtpHostConfig const config) noexcept
    {
        auto result = winrt::make_self<MidiRtpOperationResponse>();

        try
        {
            if (config == nullptr)
            {
                result->InternalSetError(rtp::MidiRtpErrorCode::InvalidArgument, internal::ResourceGetHString(IDS_RTP_SDK_ERROR_NULL_CONFIG));
                co_return *result;
            }

            auto const hostId = config.HostId();
            result->InternalSetEntryId(hostId);

            co_await winrt::resume_background();

            ApplyServiceResponse(svc::MidiServiceTransportPluginConfigManager::SendUpdate(config), *result);

            if (!result->Success()) co_return *result;

            for (uint32_t attempt = 0; attempt < HostStartPollAttempts; attempt++)
            {
                for (auto const& host : GetConfiguredHosts())
                {
                    if (host.HostId() == hostId && host.HasStarted()) co_return *result;
                }

                co_await winrt::resume_after(HostStartPollInterval);
            }

            result->InternalSetError(rtp::MidiRtpErrorCode::HostStartTimeout, internal::ResourceGetHString(IDS_RTP_SDK_ERROR_HOST_START_TIMEOUT));
        }
        catch (...)
        {
            result->InternalSetError(rtp::MidiRtpErrorCode::ClientApiException, internal::ResourceGetHString(IDS_RTP_SDK_ERROR_EXCEPTION));
        }

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> MidiRtpTransportManager::RemoveHostAsync(winrt::guid const hostId) noexcept
    {
        co_await winrt::resume_background();
        co_return RunCommand(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_HOST, { EntryArgument(hostId) }, hostId);
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> MidiRtpTransportManager::StartHostAsync(winrt::guid const hostId) noexcept
    {
        co_await winrt::resume_background();
        co_return RunCommand(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_START_HOST, { EntryArgument(hostId) }, hostId);
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> MidiRtpTransportManager::StopHostAsync(winrt::guid const hostId) noexcept
    {
        co_await winrt::resume_background();
        co_return RunCommand(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_STOP_HOST, { EntryArgument(hostId) }, hostId);
    }


    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> MidiRtpTransportManager::ConnectClientAsync(rtp::MidiRtpClientConfig const config) noexcept
    {
        auto result = winrt::make_self<MidiRtpOperationResponse>();

        try
        {
            if (config == nullptr)
            {
                result->InternalSetError(rtp::MidiRtpErrorCode::InvalidArgument, internal::ResourceGetHString(IDS_RTP_SDK_ERROR_NULL_CONFIG));
                co_return *result;
            }

            result->InternalSetEntryId(config.ClientId());

            co_await winrt::resume_background();

            ApplyServiceResponse(svc::MidiServiceTransportPluginConfigManager::SendUpdate(config), *result);
        }
        catch (...)
        {
            result->InternalSetError(rtp::MidiRtpErrorCode::ClientApiException, internal::ResourceGetHString(IDS_RTP_SDK_ERROR_EXCEPTION));
        }

        co_return *result;
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> MidiRtpTransportManager::RemoveClientAsync(winrt::guid const clientId) noexcept
    {
        co_await winrt::resume_background();
        co_return RunCommand(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_CLIENT, { EntryArgument(clientId) }, clientId);
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> MidiRtpTransportManager::ReconnectClientAsync(winrt::guid const clientId) noexcept
    {
        co_await winrt::resume_background();
        co_return RunCommand(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_RECONNECT_CLIENT, { EntryArgument(clientId) }, clientId);
    }


    _Use_decl_annotations_
    foundation::IAsyncOperation<rtp::MidiRtpOperationResponse> MidiRtpTransportManager::DisconnectConnectionAsync(winrt::guid const entryId, uint32_t const connectionId) noexcept
    {
        co_await winrt::resume_background();

        co_return RunCommand(
            MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DISCONNECT_REMOTE,
            { EntryArgument(entryId), { MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_CONNECTION_ID, winrt::to_hstring(connectionId) } },
            entryId);
    }


    collections::IVectorView<rtp::MidiRtpConfiguredHost> MidiRtpTransportManager::GetConfiguredHosts() noexcept
    {
        auto results = winrt::single_threaded_vector<rtp::MidiRtpConfiguredHost>();

        try
        {
            // one entry the SDK cannot read costs that entry, not the whole list
            for (auto const& entry : RtpSdkJson::Objects(RtpSdkJson::Array(Query(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_HOSTS), MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY)))
            {
                auto host = winrt::make_self<MidiRtpConfiguredHost>();
                if (host->InternalInitialize(entry)) results.Append(*host);
            }
        }
        catch (...)
        {
        }

        return results.GetView();
    }

    collections::IVectorView<rtp::MidiRtpConfiguredClient> MidiRtpTransportManager::GetConfiguredClients() noexcept
    {
        auto results = winrt::single_threaded_vector<rtp::MidiRtpConfiguredClient>();

        try
        {
            for (auto const& entry : RtpSdkJson::Objects(RtpSdkJson::Array(Query(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS), MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY)))
            {
                auto client = winrt::make_self<MidiRtpConfiguredClient>();
                if (client->InternalInitialize(entry)) results.Append(*client);
            }
        }
        catch (...)
        {
        }

        return results.GetView();
    }

    collections::IVectorView<rtp::MidiRtpAdvertisedPeer> MidiRtpTransportManager::GetAdvertisedPeers() noexcept
    {
        auto results = winrt::single_threaded_vector<rtp::MidiRtpAdvertisedPeer>();

        try
        {
            for (auto const& entry : RtpSdkJson::Objects(RtpSdkJson::Array(Query(MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_ADVERTISED), MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISED_PEERS_KEY)))
            {
                auto peer = winrt::make_self<MidiRtpAdvertisedPeer>();
                peer->InternalInitialize(entry);
                results.Append(*peer);
            }
        }
        catch (...)
        {
        }

        return results.GetView();
    }
}
