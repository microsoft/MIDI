// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiService.g.cpp"

namespace MIDI_CPP_NAMESPACE::implementation
{
    // returns True if the MIDI Service is available on this PC
    bool MidiService::EnsureAvailable()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        // We may want other ways to check this in the future. Need to find the most robust approaches

        try
        {
            auto serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);
            //auto serviceAbstraction = winrt::try_create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            // winrt::try_create_instance indicates failure by returning an empty com ptr
            if (serviceAbstraction == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Error contacting service. Service abstraction is nullptr.");
                return false;
            }

            winrt::com_ptr<IMidiSessionTracker> tracker;

            auto sessionTrackerResult = serviceAbstraction->Activate(__uuidof(IMidiSessionTracker), (void**)&tracker);
            if (FAILED(sessionTrackerResult))
            {
                internal::LogHresultError(__FUNCTION__, L"Failure hresult received activating interface", sessionTrackerResult);
                return false;
            }

            auto verifyConnectivityResult = tracker->VerifyConnectivity();
            if (FAILED(verifyConnectivityResult))
            {
                internal::LogHresultError(__FUNCTION__, L"Failure hresult received verifying connectivity", verifyConnectivityResult);
                return false;
            }

            internal::LogInfo(__FUNCTION__, L"Service connectivity verified");

            return true;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Error contacting service. It may be unavailable.");

            // winrt::create_instance fails by throwing an exception
            return false;
        }
    }


    _Use_decl_annotations_
    midi2::MidiServicePingResponseSummary MidiService::PingService(
        uint8_t const pingCount, 
        uint32_t timeoutMilliseconds) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        auto responseSummary = winrt::make_self<implementation::MidiServicePingResponseSummary>();

        try
        {
            if (responseSummary == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not create response summary.");

                // just need to fail
                return nullptr;
            }

            if (pingCount == 0)
            {
                internal::LogGeneralError(__FUNCTION__, L"Ping count is zero");

                responseSummary->InternalSetFailed(L"Ping count is zero.");
                return *responseSummary;
            }

            std::vector<winrt::com_ptr<implementation::MidiServicePingResponse>> pings{};
            pings.resize(pingCount);

            if (timeoutMilliseconds == 0)
            {
                internal::LogGeneralError(__FUNCTION__, L"Timeout milliseconds is zero");

                responseSummary->InternalSetFailed(L"Timeout milliseconds is zero.");
                return *responseSummary;
            }

            // we use the full session API from here to get accurate timing

            auto session = midi2::MidiSession::TryCreate(L"Ping Test");

            if (session == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to create session");

                responseSummary->InternalSetFailed(L"Unable to create session.");
                return *responseSummary;
            }

            // This ID must be consistent with what the service is set up to use.

            auto endpoint = session.TryCreateEndpointConnection(MIDI_DIAGNOSTICS_PING_BIDI_ID, false);

            if (endpoint == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to create ping endpoint");

                responseSummary->InternalSetFailed(L"Unable to create ping endpoint.");
                return *responseSummary;
            }

            // originally I was going to use a random number for this, but using the low bits of
            // the timestamp makes more sense, and will be unique enough for this.

            uint32_t pingSourceId = (uint32_t)(MidiClock::Now() & 0x00000000FFFFFFFF);

            wil::unique_event_nothrow allMessagesReceived;
            allMessagesReceived.create(wil::EventOptions::ManualReset);

            uint8_t receivedCount{ 0 };

            // we have the session and the endpoint, so set up the ping handler
            // changing any of this internal implementation detail requires a coordinated change with the server code
            // this is not an official UMP ping message. This is just an internal representation that is
            // recognized only by this endpoint, and should never be used elsewhere.

            auto MessageReceivedHandler = [&](foundation::IInspectable const& /*sender*/, midi2::MidiMessageReceivedEventArgs const& args)
                {
                    internal::MidiTimestamp actualReceiveEventTimestamp = MidiClock::Now();

                    internal::LogInfo(__FUNCTION__, L"Ping MessageReceivedHandler received message");

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
                                internal::LogInfo(__FUNCTION__, L"Setting 'allMessagesReceived' event.");

                                allMessagesReceived.SetEvent();
                            }
                        }
                        else
                        {
                            // something really wrong happened. Our index has been messed up.

                            internal::LogGeneralError(__FUNCTION__, L"Index of ping response is out of bounds");
                        }
                    }
                    else
                    {
                        // someone else is sending stuff to this ping service. Naughty if not another ping.
                        internal::LogGeneralError(__FUNCTION__, L"Another process is sending messages to this endpoint");
                    }
                };

            // any failures after this need to revoke the event handler as well
            auto eventRevokeToken = endpoint.MessageReceived(winrt::auto_revoke, MessageReceivedHandler);

            // open the endpoint. We've already set options for it not to send out discovery messages
            if (!endpoint.Open())
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not open ping endpoint.");

                responseSummary->InternalSetFailed(L"Endpoint open failed. The service may be unavailable.");

                session.DisconnectEndpointConnection(endpoint.ConnectionId());

                return *responseSummary;
            }

            // send out the ping messages

            internal::LogInfo(__FUNCTION__, L"Sending ping messages");

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

                auto sendMessageResult = endpoint.SendSingleMessageWords(timestamp, request.Word0, pingSourceId, pingIndex, request.Padding);

                if (midi2::MidiEndpointConnection::SendMessageFailed(sendMessageResult))
                {
                    internal::LogGeneralError(__FUNCTION__, L"Ping send message failed.");

                    responseSummary->InternalSetFailed(L"Sending message failed");

                    session.DisconnectEndpointConnection(endpoint.ConnectionId());

                    return *responseSummary;
                }

                //Sleep(0);
            }

            bool allReceivedFlag = allMessagesReceived.is_signaled();

            if (!allReceivedFlag)
            {
                internal::LogInfo(__FUNCTION__, L"Waiting for responses to come in");

                // Wait for all responses to come in (receivedCount == pingCount). If not all responses come back, report the failure.
                allReceivedFlag = allMessagesReceived.wait(timeoutMilliseconds);

                if (!allReceivedFlag)
                {
                    responseSummary->InternalSetFailed(L"Not all ping responses received within appropriate time window.");
                    internal::LogGeneralError(__FUNCTION__, L"Not all ping responses received within appropriate time window.");
                }
            }

            allMessagesReceived.reset();

            if (allReceivedFlag)
            {
                internal::LogInfo(__FUNCTION__, L"All ping messages received");

                // all received
                responseSummary->InternalSetSucceeded();

                // copy over the holding array into the response and also calculate the totals

                uint64_t totalPing{ 0 };

                internal::LogInfo(__FUNCTION__, L"Calculating delta timestamps");

                for (const auto& response : pings)
                {
                   // internal::LogInfo(__FUNCTION__, L"Calculating total ping");

                    totalPing += response->ClientDeltaTimestamp();

                    responseSummary->InternalAddResponse(*response);

                    // does I need to remove the com_ptr ref or will going out of scope be sufficient?
                }

                internal::LogInfo(__FUNCTION__, L"Calculating average ping");

                uint64_t averagePing = totalPing / responseSummary->Responses().Size();

                responseSummary->InternalSetTotals(totalPing, averagePing);
            }

            session.DisconnectEndpointConnection(endpoint.ConnectionId());
            session.Close();

            internal::LogInfo(__FUNCTION__, L"Returning response summary");

            return *responseSummary;
        }
        catch (std::exception ex)
        {
            internal::LogStandardExceptionError(__FUNCTION__, L"Exception pinging service.", ex);

            if (responseSummary != nullptr)
            {
                responseSummary->InternalSetFailed(winrt::to_hstring(ex.what()));

                return *responseSummary;
            }
            else
            {
                return nullptr;
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception pinging service.");

            if (responseSummary != nullptr)
            {
                responseSummary->InternalSetFailed(L"Exception pinging service");

                return *responseSummary;
            }
            else
            {
                return nullptr;
            }
        }
    }


    _Use_decl_annotations_
    midi2::MidiServicePingResponseSummary MidiService::PingService(uint8_t const pingCount) noexcept
    {
        return PingService(pingCount, pingCount * 20 + 1000);
    }



    foundation::Collections::IVector<midi2::MidiServiceTransportPluginInfo> MidiService::GetInstalledTransportPlugins()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        auto transportList = winrt::single_threaded_vector<midi2::MidiServiceTransportPluginInfo>();

        try
        {
            winrt::com_ptr<IMidiAbstraction> serviceAbstraction;
            winrt::com_ptr<IMidiServicePluginMetadataReporterInterface> metadataReporter;

            serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            if (serviceAbstraction != nullptr)
            {
                if (SUCCEEDED(serviceAbstraction->Activate(__uuidof(IMidiServicePluginMetadataReporterInterface), (void**)&metadataReporter)))
                {
                    CComBSTR metadataListJson;
                    metadataListJson.Empty();

                    metadataReporter->GetAbstractionList(&metadataListJson);

                    // parse it into json objects

                    if (metadataListJson.m_str != nullptr && metadataListJson.Length() > 0)
                    {
                        winrt::hstring hstr(metadataListJson, metadataListJson.Length());

                        // Parse the json, create the objects, throw them into the vector and return

                        json::JsonObject jsonObject = json::JsonObject::Parse(hstr);

                        if (jsonObject != nullptr)
                        {
                            for (auto const& transportKV : jsonObject)
                            {
                                auto info = winrt::make_self<implementation::MidiServiceTransportPluginInfo>();

                                auto transport = transportKV.Value().GetObject();

                                info->InternalInitialize(
                                    internal::StringToGuid(transportKV.Key().c_str()),
                                    transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_NAME_PROPERTY_KEY, L""),
                                    transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_MNEMONIC_PROPERTY_KEY, L""),
                                    transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_DESCRIPTION_PROPERTY_KEY, L""),
                                    transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_SMALL_IMAGE_PATH_PROPERTY_KEY, L""),
                                    transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_AUTHOR_PROPERTY_KEY, L""),
                                    transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_VERSION_PROPERTY_KEY, L""),
                                    transport.GetNamedBoolean(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_SYSTEM_MANAGED_PROPERTY_KEY, false),
                                    transport.GetNamedBoolean(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_RT_CREATABLE_APPS_PROPERTY_KEY, false),
                                    transport.GetNamedBoolean(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_RT_CREATABLE_SETTINGS_PROPERTY_KEY, false),
                                    transport.GetNamedBoolean(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_CLIENT_CONFIGURABLE_PROPERTY_KEY, false),
                                    transport.GetNamedString(MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_CLIENT_CONFIG_ASSEMBLY_PROPERTY_KEY, L"")
                                );

                                transportList.Append(*info);
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

        return transportList;
    }


    foundation::Collections::IVector<midi2::MidiServiceMessageProcessingPluginInfo> MidiService::GetInstalledMessageProcessingPlugins()
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        // TODO: Need to implement GetInstalledMessageProcessingPlugins. For now, return an empty collection instead of throwing

        // This can be read from the registry, but the additional metadata requires calling into the objects themselves

        return winrt::single_threaded_vector<midi2::MidiServiceMessageProcessingPluginInfo>();
    }

    foundation::Collections::IVector<midi2::MidiServiceSessionInfo> MidiService::GetActiveSessions() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        auto sessionList = winrt::single_threaded_vector<midi2::MidiServiceSessionInfo>();

        //try
        //{
        //    winrt::com_ptr<IMidiAbstraction> serviceAbstraction;
        //    winrt::com_ptr<IMidiSessionTracker> sessionTracker;

        //    serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

        //    if (serviceAbstraction != nullptr)
        //    {
        //        if (SUCCEEDED(serviceAbstraction->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker)))
        //        {
        //            CComBSTR sessionListJson;
        //            sessionListJson.Empty();

        //            sessionTracker->GetSessionListJson(&sessionListJson);

        //            // parse it into json objects

        //            if (sessionListJson.m_str != nullptr && sessionListJson.Length() > 0)
        //            {
        //                winrt::hstring hstr(sessionListJson, sessionListJson.Length());

        //                // Parse the json, create the objects, throw them into the vector and return

        //                json::JsonObject jsonObject = json::JsonObject::Parse(hstr);

        //                if (jsonObject != nullptr)
        //                {
        //                    auto sessionJsonArray = jsonObject.GetNamedArray(MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ARRAY_PROPERTY_KEY);

        //                    GUID defaultGuid{};
        //                    std::chrono::time_point<std::chrono::system_clock> noTime;

        //                    for (uint32_t i = 0; i < sessionJsonArray.Size(); i++)
        //                    {
        //                        auto sessionJson = sessionJsonArray.GetObjectAt(i);
        //                        auto sessionObject = winrt::make_self<implementation::MidiServiceSessionInfo>();

        //                        //    auto startTimeString = internal::JsonGetWStringProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, L"").c_str();

        //                        auto startTime = internal::JsonGetDateTimeProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY, noTime);

        //                        sessionObject->InternalInitialize(
        //                            internal::JsonGetGuidProperty(sessionJson, MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ID_PROPERTY_KEY, defaultGuid),
        //                            sessionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_NAME_PROPERTY_KEY, L"").c_str(),
        //                            std::stol(sessionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_ID_PROPERTY_KEY, L"0").c_str()),
        //                            sessionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_NAME_PROPERTY_KEY, L"").c_str(),
        //                            winrt::clock::from_sys(startTime)
        //                        );


        //                        // Add connections

        //                        auto connectionsJsonArray = sessionJson.GetNamedArray(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ARRAY_PROPERTY_KEY, nullptr);

        //                        if (connectionsJsonArray != nullptr && connectionsJsonArray.Size() > 0)
        //                        {
        //                            for (uint32_t j = 0; j < connectionsJsonArray.Size(); j++)
        //                            {
        //                                auto connectionJson = connectionsJsonArray.GetObjectAt(j);
        //                                auto connectionObject = winrt::make_self<implementation::MidiServiceSessionConnectionInfo>();

        //                                auto earliestConnectionTime = internal::JsonGetDateTimeProperty(connectionJson, MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_TIME_PROPERTY_KEY, noTime);

        //                                connectionObject->InternalInitialize(
        //                                    connectionJson.GetNamedString(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ENDPOINT_ID_PROPERTY_KEY, L"").c_str(),
        //                                    (uint16_t)(connectionJson.GetNamedNumber(MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_COUNT_PROPERTY_KEY, 0)),
        //                                    winrt::clock::from_sys(earliestConnectionTime)
        //                                );

        //                                sessionObject->InternalAddConnection(*connectionObject);
        //                            }
        //                        }

        //                        sessionList.Append(*sessionObject);
        //                    }
        //                }
        //            }
        //        }
        //    }
        //}
        //catch (...)
        //{
        //    internal::LogGeneralError(__FUNCTION__, L"Exception processing session tracker result json");
        //}

        return sessionList;

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
        winrt::guid loopbackDeviceAbstractionId = internal::StringToGuid(L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}");


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
        //        "{associationGuid}":
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

        endpointDeviceAObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            json::JsonValue::CreateStringValue(endpointDefinitionA.Name().c_str()));

        endpointDeviceAObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            json::JsonValue::CreateStringValue(endpointDefinitionA.Description().c_str()));

        endpointDeviceAObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            json::JsonValue::CreateStringValue(endpointDefinitionA.UniqueId().c_str()));

        //MIDI_CONFIG_JSON_ENDPOINT_COMMON_MANUFACTURER_PROPERTY


        // build Endpoint B

        endpointDeviceBObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            json::JsonValue::CreateStringValue(endpointDefinitionB.Name().c_str()));

        endpointDeviceBObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            json::JsonValue::CreateStringValue(endpointDefinitionB.Description().c_str()));

        endpointDeviceBObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            json::JsonValue::CreateStringValue(endpointDefinitionB.UniqueId().c_str()));


        // create the association object with the two devices as children

        endpointAssociationObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_A_KEY,
            endpointDeviceAObject);

        endpointAssociationObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_B_KEY,
            endpointDeviceBObject);

        // create the creation node with the association object as the child property

        endpointCreationObject.SetNamedValue(
            internal::GuidToString(associationId),
            endpointAssociationObject);

        // create the abstraction object with the child creation node

        abstractionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY,
            endpointCreationObject);

        // create the main node

        topLevelTransportPluginSettingsObject.SetNamedValue(
            internal::GuidToString(loopbackDeviceAbstractionId),
            abstractionObject);


        // wrap it all up so the json is valid

        wrapperObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);

        // send it up

        json::JsonObject responseObject = InternalSendConfigurationJsonAndGetResponse(loopbackDeviceAbstractionId, wrapperObject, false);

        // parse the results
        auto successResult = responseObject.GetNamedBoolean(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, false);

        if (successResult)
        {
            internal::LogInfo(__FUNCTION__, L"JSON payload indicates success");

            auto deviceIdA = responseObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_A_ID_KEY, L"");
            auto deviceIdB = responseObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_B_ID_KEY, L"");

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
            internal::LogGeneralError(__FUNCTION__, L"Device creation failed (payload has false success value)");
        }

        return *result;
    }


    _Use_decl_annotations_
    json::JsonObject MidiService::InternalSendConfigurationJsonAndGetResponse(
        winrt::guid const& abstractionId, 
        json::JsonObject const& configObject,
        bool const isFromConfigurationFile
    ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");


        auto iid = __uuidof(IMidiAbstractionConfigurationManager);
        winrt::com_ptr<IMidiAbstractionConfigurationManager> configManager;

        auto serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

        // default to failed
        auto response = internal::BuildConfigurationResponseObject(false);


        if (serviceAbstraction)
        {
            auto activateConfigManagerResult = serviceAbstraction->Activate(iid, (void**)&configManager);

            internal::LogInfo(__FUNCTION__, L"config manager activate call completed");

            if (FAILED(activateConfigManagerResult) || configManager == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Config manager is null or call failed.");

                return response;
            }

            internal::LogInfo(__FUNCTION__, L"Config manager activate call SUCCESS");

            auto initializeResult = configManager->Initialize(abstractionId, nullptr, nullptr);

            if (FAILED(initializeResult))
            {
                internal::LogGeneralError(__FUNCTION__, L"Failed to initialize config manager");

                // return a fail result
                return response;
            }

            CComBSTR responseString{};
            responseString.Empty();

            auto jsonPayload = configObject.Stringify();

            // send up the payload

            internal::LogInfo(__FUNCTION__, jsonPayload.c_str());
            auto configUpdateResult = configManager->UpdateConfiguration(jsonPayload.c_str(), isFromConfigurationFile, &responseString);

            internal::LogInfo(__FUNCTION__, responseString);


            if (FAILED(configUpdateResult))
            {
                internal::LogGeneralError(__FUNCTION__, L"Failed to configure endpoint");

                // return a failed result
                return response;
            }
            else
            {
                json::JsonObject responseObject;

                if (internal::JsonObjectFromBSTR(&responseString, responseObject))
                {
                    return responseObject;
                }
                else
                {
                    // failed

                    internal::LogGeneralError(__FUNCTION__, L"Unable to read response object from BSTR");

                    return response;
                }
            }
        }
        else
        {
            // failed
            internal::LogGeneralError(__FUNCTION__, L"Failed to create service abstraction");

            return response;
        }

    }


    _Use_decl_annotations_
    bool MidiService::RemoveTemporaryLoopbackEndpoints(_In_ winrt::guid const& associationId) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        // todo: grab this from a constant
        winrt::guid loopbackDeviceAbstractionId = internal::StringToGuid(L"{942BF02D-93C0-4EA8-B03E-D51156CA75E1}");

        json::JsonObject wrapperObject;
        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject abstractionObject;
        json::JsonArray endpointDeletionArray;
        
        internal::LogInfo(__FUNCTION__, L" setting json properties");

        // "endpointTransportPluginSettings":
        // {
        //   loopback endpoint abstraction guid :
        //   {
        //     "remove"
        //     {
        //        "{associationGuid}"
        //     }
        //   }
        // }


        // create the deletion node with the association object as the child property

        json::JsonValue endpointDeletionAssociationId = json::JsonValue::CreateStringValue(internal::GuidToString(associationId).c_str());

        endpointDeletionArray.Append(endpointDeletionAssociationId);

        // create the abstraction object with the child creation node

        abstractionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY,
            endpointDeletionArray);

        // create the main node

        topLevelTransportPluginSettingsObject.SetNamedValue(
            internal::GuidToString(loopbackDeviceAbstractionId),
            abstractionObject);


        // wrap it all up so the json is valid

        wrapperObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);

        // send it up

        internal::LogInfo(__FUNCTION__, L"configManager->UpdateConfiguration success");

        json::JsonObject responseObject = InternalSendConfigurationJsonAndGetResponse(loopbackDeviceAbstractionId, wrapperObject, false);



        // parse the results
        auto successResult = responseObject.GetNamedBoolean(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, false);

        if (successResult)
        {
            internal::LogInfo(__FUNCTION__, L"JSON payload indicates success");
        }
        else
        {
            internal::LogGeneralError(__FUNCTION__, L"Loopback device removal failed (payload has false success value)");
        }

        return successResult;
    }



    _Use_decl_annotations_
    midi2::MidiServiceConfigurationResponse MidiService::UpdateTransportPluginConfiguration(
        midi2::IMidiServiceTransportPluginConfiguration const& configurationUpdate) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");
        
        // this initializes to a failure state, so we can just return it when we have a fail
        auto response = winrt::make_self<implementation::MidiServiceConfigurationResponse>();
        response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorOther); // default

        if (configurationUpdate == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"Configuration object is null");
            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorOther);

            return *response;
        }

        json::JsonObject jsonUpdate{ nullptr }; 
            
        if (!json::JsonObject::TryParse(configurationUpdate.SettingsJson(), jsonUpdate))
        {
            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorProcessingJson);
            return *response;
        }

        if (jsonUpdate == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"Configuration object SettingsJson is null");

            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorProcessingJson);
            return *response;
        }

        auto responseJsonObject = InternalSendConfigurationJsonAndGetResponse(
            configurationUpdate.TransportId(),
            jsonUpdate,
            configurationUpdate.IsFromConfigurationFile()
        );

        if (responseJsonObject == nullptr)
        {
            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorProcessingJson);
            return *response;
        }

        auto success = responseJsonObject.GetNamedBoolean(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, false);

        if (success)
        {
            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::Success);

            // TODO: Process the return object

            return *response;
        }
        else
        {
            response->InternalSetStatus(midi2::MidiServiceConfigurationResponseStatus::ErrorOther);

            return *response;
        }
    }

    _Use_decl_annotations_
    midi2::MidiServiceConfigurationResponse MidiService::UpdateProcessingPluginConfiguration(
        midi2::IMidiServiceMessageProcessingPluginConfiguration const& configurationUpdate) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");
        
        UNREFERENCED_PARAMETER(configurationUpdate);
        // initializes to a failed value
        auto response = winrt::make_self<implementation::MidiServiceConfigurationResponse>();




        // TODO: Implement this in service and API





        return *response;
    }


}
