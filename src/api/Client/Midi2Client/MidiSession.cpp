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
#include <atlcomcli.h>


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
                internal::LogGeneralError(__FUNCTION__, L"WinRT Endpoint connection wouldn't start");

                // TODO: Cleanup

                return nullptr;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception connecting to endpoint. Service or endpoint may be unavailable, or endpoint may not be the correct type.", ex);

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
        midi2::MidiVirtualEndpointDeviceDefinition const& deviceDefinition
    ) noexcept
    {
        OutputDebugString(__FUNCTION__ L"");

        //winrt::hstring endpointDeviceId = 
        //    L"\\\\?\\SWD#MIDISRV#MIDIU_VIRTDEV_" + 
        //    deviceDefinition.EndpointProductInstanceId() +
        //    L"#" + midi2::MidiEndpointDeviceInformation::EndpointInterfaceClass();

        winrt::hstring endpointDeviceId{};


        // TODO: Replace all these strings with consts/defines from the service src ========================================================

        // create the json for creating the endpoint


        // todo: grab this from a constant
        winrt::hstring virtualDeviceAbstractionId = L"{8FEAAD91 -70E1-4A19-997A-377720A719C1}";

        json::JsonObject endpointCreationObject;
        json::JsonArray createVirtualDevicesArray;
        json::JsonObject endpointDefinitionObject;

        // Create association guid
        auto associationGuid = foundation::GuidHelper::CreateNewGuid();

        // set all of our properties.
        endpointDefinitionObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY, json::JsonValue::CreateStringValue(winrt::to_hstring(associationGuid)));
        endpointDefinitionObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_UNIQUE_ID_PROPERTY_KEY, json::JsonValue::CreateStringValue(deviceDefinition.EndpointProductInstanceId()));
        endpointDefinitionObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, json::JsonValue::CreateStringValue(deviceDefinition.EndpointName()));
//        endpointDefinitionObject.SetNamedValue(L"description", json::JsonValue::CreateStringValue(deviceDefinition.EndpointDescription()));

        // TODO: Other props that have to be set at the service level and not in-protocol




        createVirtualDevicesArray.Append(endpointDefinitionObject);

        json::JsonObject abstractionObject;
        abstractionObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_CREATE_ARRAY_KEY, createVirtualDevicesArray);

        endpointCreationObject.SetNamedValue(virtualDeviceAbstractionId, abstractionObject);


        // send it up

        auto iid = __uuidof(IMidiAbstractionConfigurationManager);

        winrt::com_ptr<IMidiAbstractionConfigurationManager> configManager;

        winrt::check_hresult(m_serviceAbstraction->Activate(iid, (void**)&configManager));


        if (configManager != nullptr)
        {
            winrt::check_hresult(configManager->Initialize(internal::StringToGuid(virtualDeviceAbstractionId.c_str()), nullptr));

            CComBSTR response;
            response.Empty();

            configManager->UpdateConfiguration(abstractionObject.Stringify().c_str(), &response);

            // TODO: check the response. If success, get the SWD id so we can create the connection

            json::JsonObject responseObject;

            if (internal::JsonObjectFromBSTR(&response, responseObject))
            {
                // check for success

                auto success = internal::JsonGetBoolProperty(responseObject, MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_SUCCESS_PROPERTY_KEY, false);

                if (success)
                {
                    auto responseArray = internal::JsonGetArrayProperty(responseObject, MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY);

                    if (responseArray.Size() > 0)
                    {
                        // get the id. We should have only one item in the response array here so we'll just grab the first

                        auto firstObject = responseArray.GetObjectAt(0);

                        endpointDeviceId = internal::JsonGetWStringProperty(
                            firstObject, 
                            MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY, 
                            L"");

                        if (endpointDeviceId == L"")
                        {
                            // failed in another unexpected way
                            OutputDebugString(__FUNCTION__ L" Virtual device creation failed. Created Id was empty");

                            return nullptr;
                        }
                    }
                    else
                    {
                        // creation failed in an unexpected way
                        OutputDebugString(__FUNCTION__ L" Virtual device creation failed. Empty response array");

                        return nullptr;
                    }

                    //endpointDeviceId
                }
                else
                {
                    // creation failed
                    OutputDebugString(__FUNCTION__ L" Virtual device creation failed");

                    return nullptr;
                }
            }
            else
            {
                // failed in some way
                OutputDebugString(__FUNCTION__ L" Failed to read json response object from virtual device creation");

                return nullptr;
            }



        }
        else
        {
            // failed. Log

            return nullptr;
        }



        // Creating the device succeeded, so create the connection
        auto connection = CreateEndpointConnection(endpointDeviceId, nullptr, nullptr);

        if (connection)
        {
            auto virtualDevice = winrt::make_self<implementation::MidiVirtualEndpointDevice>();

            virtualDevice->InternalSetDeviceDefinition(deviceDefinition);
            virtualDevice->IsEnabled(true);

            // add the listener
            connection.MessageProcessingPlugins().Append(*virtualDevice);
        }
        else
        {
            internal::LogGeneralError(__FUNCTION__, L"Could not create connection");
        }

        return connection;
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
                // disconnect the endpoint from the service, call Close() etc.

                m_connections.Lookup(endpointConnectionId).as<foundation::IClosable>().Close();

                m_connections.Remove(endpointConnectionId);
            }
            else
            {
                // endpoint already disconnected. No need to throw an exception or anything, just exit.
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
