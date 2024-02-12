// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiService.h"
#include "MidiService.g.cpp"

#include "ping_ump_types.h"

#include <Windows.h>
#include <wil\resource.h>

namespace winrt::Windows::Devices::Midi2::implementation
{

    _Use_decl_annotations_
        midi2::MidiServicePingResponseSummary MidiService::PingService(uint8_t const pingCount, uint32_t timeoutMilliseconds) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        auto responseSummary = winrt::make_self<implementation::MidiServicePingResponseSummary>();

        if (responseSummary == nullptr)
        {
            // just need to fail
            return nullptr;
        }

        if (pingCount == 0)
        {
            responseSummary->InternalSetFailed(L"Ping count is zero.");
            return *responseSummary;
        }

        std::vector<winrt::com_ptr<implementation::MidiServicePingResponse>> pings{};
        pings.resize(pingCount);

        if (timeoutMilliseconds == 0)
        {
            responseSummary->InternalSetFailed(L"Timeout milliseconds is zero.");
            return *responseSummary;
        }

        // we use the full session API from here to get accurate timing

        auto session = midi2::MidiSession::CreateSession(L"Ping Test");

        if (session == nullptr)
        {
            responseSummary->InternalSetFailed(L"Unable to create session.");
            return *responseSummary;
        }

        // This ID must be consistent with what the service is set up to use.

        auto endpoint = session.CreateEndpointConnection(MIDI_DIAGNOSTICS_PING_BIDI_ID);

        if (endpoint == nullptr)
        {
            responseSummary->InternalSetFailed(L"Unable to create ping endpoint.");
            return *responseSummary;
        }


        // originally I was going to use a random number for this, but using the low bits of
        // the timestamp makes more sense, and will be unique enough for this.

        uint32_t pingSourceId = (uint32_t)(MidiClock::Now() & 0x00000000FFFFFFFF);

        wil::unique_event_nothrow allMessagesReceived;
        allMessagesReceived.create();

        uint8_t receivedCount{ 0 };

        // we have the session and the endpoint, so set up the ping handler
        // changing any of this internal implementation detail requires a coordinated change with the server code
        // this is not an official UMP ping message. This is just an internal representation that is
        // recognized only by this endpoint, and should never be used elsewhere.


        auto MessageReceivedHandler = [&](foundation::IInspectable const& /*sender*/, midi2::MidiMessageReceivedEventArgs const& args)
            {
                internal::MidiTimestamp actualReceiveEventTimestamp = MidiClock::Now();

                uint32_t word0;
                uint32_t word1;
                uint32_t word2;
                uint32_t word3;

                args.FillWords(word0, word1, word2, word3);

                // ensure this is a ping message, just in case

                if (word0 == INTERNAL_PING_RESPONSE_UMP_WORD0 && word1 == pingSourceId)
                {
                    if (word2 < pings.size())
                    {
                        // word2 is our ping index
                        pings[word2]->InternalSetReceiveInfo(args.Timestamp(), actualReceiveEventTimestamp);

                        receivedCount++;

                        if (receivedCount == pingCount)
                        {
                            allMessagesReceived.SetEvent();
                        }
                    }
                    else
                    {
                        // something really wrong happened. Our index has been messed up.
                    }
                }
                else
                {
                    // someone else is sending stuff to this ping service. Naughty if not another ping.
                }

            };

        // any failures after this need to revoke the event handler as well
        auto eventRevokeToken = endpoint.MessageReceived(MessageReceivedHandler);

        // open the endpoint. We've already set options for it not to send out discovery messages
        if (!endpoint.Open())
        {
            internal::LogGeneralError(__FUNCTION__, L"Could not open ping endpoint.");

            responseSummary->InternalSetFailed(L"Endpoint open failed. The service may be unavailable.");
            endpoint.MessageReceived(eventRevokeToken);

            session.DisconnectEndpointConnection(endpoint.ConnectionId());


            return *responseSummary;
        }

        // send out the ping messages

        for (uint32_t pingIndex = 0; pingIndex < pingCount; pingIndex++)
        {
            internal::PackedPingRequestUmp request;

            auto response = winrt::make_self<implementation::MidiServicePingResponse>();

            internal::MidiTimestamp timestamp = MidiClock::Now();

            // Add this info to the tracking before we send, so no race condition
            // granted that this adds a few ticks to add this to the collection and build the object

            response->InternalSetSendInfo(pingSourceId, pingIndex, timestamp);

            //
            // TODO: Should this use copy_from?
            pings[pingIndex] = response;

            // send the ping
            endpoint.SendMessageWords(timestamp, request.Word0, pingSourceId, pingIndex, request.Padding);

            //Sleep(0);
        }

        // Wait for all responses to come in (receivedCount == pingCount). If not all responses come back, report the failure.
        if (!allMessagesReceived.wait(timeoutMilliseconds))
        {
            responseSummary->InternalSetFailed(L"Not all ping responses received within appropriate time window.");
            internal::LogGeneralError(__FUNCTION__, L"Not all ping responses received within appropriate time window.");
        }
        else
        {
            // all received
            responseSummary->InternalSetSucceeded();

            // copy over the holding array into the response and also calculate the totals

            uint64_t totalPing{ 0 };

            for (const auto& response : pings)
            {
                totalPing += response->ClientDeltaTimestamp();

                responseSummary->InternalAddResponse(*response);

                // does I need to remove the com_ptr ref or will going out of scope be sufficient?
            }

            uint64_t averagePing = totalPing / responseSummary->Responses().Size();

            responseSummary->InternalSetTotals(totalPing, averagePing);
        }

        // unwire the event and close the session.
        endpoint.MessageReceived(eventRevokeToken);

        session.DisconnectEndpointConnection(endpoint.ConnectionId());
        session.Close();

        return *responseSummary;
    }

    _Use_decl_annotations_
        midi2::MidiServicePingResponseSummary MidiService::PingService(uint8_t const pingCount) noexcept
    {
        return PingService(pingCount, pingCount * 20 + 1000);
    }



    foundation::Collections::IVectorView<midi2::MidiServiceTransportPluginInformation> MidiService::GetInstalledTransportPlugins()
    {
        // TODO: Need to implement GetInstalledTransportPlugins. For now, return an empty collection instead of throwing

        // This can be read from the registry, but the additional metadata requires calling into the objects themselves


        return winrt::single_threaded_vector<midi2::MidiServiceTransportPluginInformation>().GetView();
    }


    foundation::Collections::IVectorView<midi2::MidiServiceMessageProcessingPluginInformation> MidiService::GetInstalledMessageProcessingPlugins()
    {
        // TODO: Need to implement GetInstalledMessageProcessingPlugins. For now, return an empty collection instead of throwing

        // This can be read from the registry, but the additional metadata requires calling into the objects themselves

        return winrt::single_threaded_vector<midi2::MidiServiceMessageProcessingPluginInformation>().GetView();
    }

    foundation::Collections::IVectorView<midi2::MidiServiceSessionInformation> MidiService::GetActiveSessions() noexcept
    {
        auto sessionList = winrt::single_threaded_vector<midi2::MidiServiceSessionInformation>();

        try
        {
            winrt::com_ptr<IMidiAbstraction> serviceAbstraction;
            winrt::com_ptr<IMidiSessionTracker> sessionTracker;

            serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            if (serviceAbstraction != nullptr)
            {
                if (SUCCEEDED(serviceAbstraction->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker)))
                {
                    CComBSTR sessionListJson;
                    sessionListJson.Empty();

                    sessionTracker->GetSessionListJson(&sessionListJson);

                    // parse it into json objects

                    if (sessionListJson != nullptr && sessionListJson.Length() > 0)
                    {
                        winrt::hstring hstr(sessionListJson, sessionListJson.Length());

                        // Parse the json, create the objects, throw them into the vector and return

                        json::JsonObject jsonObject = json::JsonObject::Parse(hstr);

                        if (jsonObject != nullptr)
                        {
                            auto sessionJsonArray = internal::JsonGetArrayProperty(jsonObject, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ARRAY_PROPERTY_KEY);

                            GUID defaultGuid{};
                            std::chrono::time_point<std::chrono::system_clock> noTime;

                            for (uint32_t i = 0; i < sessionJsonArray.Size(); i++)
                            {
                                auto sessionJson = sessionJsonArray.GetObjectAt(i);
                                auto sessionObject = winrt::make_self<implementation::MidiServiceSessionInformation>();

                                //    auto startTimeString = internal::JsonGetWStringProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, L"").c_str();

                                auto startTime = internal::JsonGetDateTimeProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, noTime);

                                sessionObject->InternalInitialize(
                                    internal::JsonGetGuidProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ID_PROPERTY_KEY, defaultGuid),
                                    internal::JsonGetWStringProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_NAME_PROPERTY_KEY, L"").c_str(),
                                    std::stol(internal::JsonGetWStringProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_ID_PROPERTY_KEY, L"0")),
                                    internal::JsonGetWStringProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_NAME_PROPERTY_KEY, L"").c_str(),
                                    winrt::clock::from_sys(startTime)
                                );


                                // Add connections

                                auto connectionsJsonArray = internal::JsonGetArrayProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ARRAY_PROPERTY_KEY);

                                if (connectionsJsonArray.Size() > 0)
                                {
                                    for (uint32_t j = 0; j < connectionsJsonArray.Size(); j++)
                                    {
                                        auto connectionJson = connectionsJsonArray.GetObjectAt(j);
                                        auto connectionObject = winrt::make_self<implementation::MidiServiceSessionConnectionInformation>();

                                        auto earliestConnectionTime = internal::JsonGetDateTimeProperty(connectionJson, MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_TIME_PROPERTY_KEY, noTime);

                                        connectionObject->InternalInitialize(
                                            internal::JsonGetWStringProperty(connectionJson, MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ENDPOINT_ID_PROPERTY_KEY, L"").c_str(),
                                            (uint16_t)(internal::JsonGetDoubleProperty(connectionJson, MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_COUNT_PROPERTY_KEY, 0)),
                                            winrt::clock::from_sys(earliestConnectionTime)
                                        );

                                        sessionObject->InternalAddConnection(*connectionObject);
                                    }
                                }

                                sessionList.Append(*sessionObject);
                            }
                        }
                    }
                }
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception processing session tracker result json");
        }

        return sessionList.GetView();

    }

    _Use_decl_annotations_
    midi2::MidiServiceLoopbackEndpointCreationResult MidiService::CreateTemporaryLoopbackEndpoints(
        winrt::guid const& associationId,
        midi2::MidiServiceLoopbackEndpointDefinition const& endpointDefinitionA,
        midi2::MidiServiceLoopbackEndpointDefinition const& endpointDefinitionB) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        // the success code in this defaults to False
        auto result = winrt::make_self<implementation::MidiServiceLoopbackEndpointCreationResult>();

        // todo: grab this from a constant
        winrt::hstring loopbackDeviceAbstractionId = L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}";


        json::JsonObject wrapperObject;
        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject abstractionObject;
        json::JsonObject endpointCreationObject;

        json::JsonObject endpointAssociationObject;
        json::JsonObject endpointDeviceAObject;
        json::JsonObject endpointDeviceBObject;

        internal::LogInfo(__FUNCTION__, L" setting json properties");

        // "endpointTransportPluginSettings":
        // {
        //   endpoint abstraction guid :
        //   {
        //     "create"
        //     {
        //        associationGuid:
        //        {
        //            "endpointA":
        //            {
        //               ... endpoint properties ...
        //            },
        //            "endpointB":
        //            {
        //               ... endpoint properties ...
        //            }
        //        }
        //     }
        //   }
        // }

        // build Endpoint A

        internal::JsonGetWStringProperty(
            endpointDeviceAObject,
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            endpointDefinitionA.Name().c_str());

        internal::JsonGetWStringProperty(
            endpointDeviceAObject,
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            endpointDefinitionA.Description().c_str());

        internal::JsonGetWStringProperty(
            endpointDeviceAObject,
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            endpointDefinitionA.UniqueId().c_str());

        // build Endpoint B

        internal::JsonGetWStringProperty(
            endpointDeviceBObject,
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            endpointDefinitionB.Name().c_str());

        internal::JsonGetWStringProperty(
            endpointDeviceBObject,
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            endpointDefinitionB.Description().c_str());

        internal::JsonGetWStringProperty(
            endpointDeviceBObject,
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            endpointDefinitionB.UniqueId().c_str());

        // create the association object with the two devices as children

        internal::JsonSetObjectProperty(
            endpointAssociationObject,
            MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_A_KEY,
            endpointDeviceAObject);

        internal::JsonSetObjectProperty(
            endpointAssociationObject,
            MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_B_KEY,
            endpointDeviceBObject);

        // create the creation node with the association object as the child property

        internal::JsonSetObjectProperty(
            endpointCreationObject,
            internal::GuidToString(associationId),
            endpointAssociationObject);

        // create the abstraction object with the child creation node

        internal::JsonSetObjectProperty(
            abstractionObject,
            MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_CREATE_KEY,
            endpointCreationObject);

        // create the main node

        internal::JsonSetObjectProperty(
            topLevelTransportPluginSettingsObject,
            loopbackDeviceAbstractionId.c_str(),
            abstractionObject);


        // wrap it all up so the json is valid

        internal::JsonSetObjectProperty(
            wrapperObject,
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);

        // send it up


        auto iid = __uuidof(IMidiAbstractionConfigurationManager);
        winrt::com_ptr<IMidiAbstractionConfigurationManager> configManager;

        auto serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

        if (serviceAbstraction)
        {
            auto activateConfigManagerResult = serviceAbstraction->Activate(iid, (void**)&configManager);

            internal::LogInfo(__FUNCTION__, L"config manager activate call completed");


            if (FAILED(activateConfigManagerResult) || configManager == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Failed to create device. Config manager is null or call failed.");

                // return a fail result
                return *result;
            }

            internal::LogInfo(__FUNCTION__, L"config manager activate call SUCCESS");

            auto initializeResult = configManager->Initialize(internal::StringToGuid(loopbackDeviceAbstractionId.c_str()), nullptr);


            if (FAILED(initializeResult))
            {
                internal::LogGeneralError(__FUNCTION__, L"failed to initialize config manager");

                // return a fail result
                return *result;
            }

            CComBSTR response{};
            response.Empty();

            auto jsonPayload = wrapperObject.Stringify();

            internal::LogInfo(__FUNCTION__, jsonPayload.c_str());
            auto configUpdateResult = configManager->UpdateConfiguration(jsonPayload.c_str(), false, &response);

            if (FAILED(configUpdateResult))
            {
                internal::LogGeneralError(__FUNCTION__, L"Failed to configure endpoint");

                // return a failed result
                return *result;
            }

            internal::LogInfo(__FUNCTION__, L"configManager->UpdateConfiguration success");

            json::JsonObject responseObject;

            if (!internal::JsonObjectFromBSTR(&response, responseObject))
            {
                internal::LogGeneralError(__FUNCTION__, L"Failed to read json response object from loopback device creation");

                // return a failed result
                return *result;
            }

            internal::LogInfo(__FUNCTION__, L"JsonObjectFromBSTR success");




            // check for actual success
            auto successResult = internal::JsonGetBoolProperty(responseObject, MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_SUCCESS_PROPERTY_KEY, false);

            if (successResult)
            {
                internal::LogInfo(__FUNCTION__, L"JSON payload indicates success");


                // TODO: A and B are simple properties here. We don't need
                // an array because we create one at a time through the API. And when
                // created through the config file, there's no response object to 
                // worry about.

                auto deviceIdA = internal::JsonGetWStringProperty(responseObject, MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_A_ID_KEY, L"");
                auto deviceIdB = internal::JsonGetWStringProperty(responseObject, MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_B_ID_KEY, L"");

                if (deviceIdA.empty())
                {
                    internal::LogGeneralError(__FUNCTION__, L"Unexpected empty Device Id A");

                    return *result;
                }

                if (deviceIdB.empty())
                {
                    internal::LogGeneralError(__FUNCTION__, L"Unexpected empty Device Id B");

                    return *result;
                }


                // update the response object with the new ids

                result->SetSuccess(associationId, deviceIdA.c_str(), deviceIdB.c_str());
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Loopback device creation failed (payload has false success value)");

                return nullptr;
            }

            internal::LogInfo(__FUNCTION__, L"Loopback device creation worked.");

        }
        else
        {
            // failed
            internal::LogGeneralError(__FUNCTION__, L"Failed to create service abstraction");

        }

        return *result;
    }

    _Use_decl_annotations_
    bool MidiService::RemoveTemporaryLoopbackEndpoints(_In_ winrt::guid const& associationId) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        UNREFERENCED_PARAMETER(associationId);
        // TODO:

        return false;
    }

    _Use_decl_annotations_
    midi2::MidiServiceConfigurationResponse MidiService::UpdateTransportPluginConfiguration(
        midi2::IMidiServiceTransportPluginConfiguration const& configurationUpdate) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");
        
        UNREFERENCED_PARAMETER(configurationUpdate);
        // TODO:

        auto response = winrt::make_self<implementation::MidiServiceConfigurationResponse>();

        return *response;

    }

    _Use_decl_annotations_
    midi2::MidiServiceConfigurationResponse MidiService::UpdateProcessingPluginConfiguration(
        midi2::IMidiServiceMessageProcessingPluginConfiguration const& configurationUpdate) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");
        
        UNREFERENCED_PARAMETER(configurationUpdate);
        // TODO:

        auto response = winrt::make_self<implementation::MidiServiceConfigurationResponse>();

        return *response;
    }


}
