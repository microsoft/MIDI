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

#include <atlcomcli.h>

#include <filesystem>

namespace MIDI_CPP_NAMESPACE::implementation
{

    _Use_decl_annotations_
    midi2::MidiSession MidiSession::CreateSession(
        winrt::hstring const& sessionName,
        midi2::MidiSessionSettings const& settings
        ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Session create");

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
                // this can happen is service is not available or running

                internal::LogInfo(__FUNCTION__, L"Session start failed. Returning nullptr");

                return nullptr;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception initializing creating session. Service may be unavailable.", ex);

            return nullptr;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception initializing creating session. Service may be unavailable.");

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
        internal::LogInfo(__FUNCTION__, L"Start Session ");

        try
        {
            // We're talking to the service, so use the MIDI Service abstraction, not a KS or other one
            m_serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            if (m_serviceAbstraction != nullptr)
            {
                // the session id is created on the client and provided in calls to the endpoints
                m_id = foundation::GuidHelper::CreateNewGuid();

                m_isOpen = true;


                // register the session

                if (SUCCEEDED(m_serviceAbstraction->Activate(__uuidof(IMidiSessionTracker), (void**)&m_sessionTracker)))
                {
                    m_sessionTracker->Initialize();

                    DWORD clientProcessId = GetCurrentProcessId();

                    //std::wstring modulePath{ _wpgmptr };

                    std::wstring modulePath{ 0 };
                    modulePath.resize(2048);   // MAX_PATH is almost never big enough. This is a wild shot. Not going to allocate 32k chars for this but I know this will bite me some day

                    auto numPathCharacters = GetModuleFileName(NULL, modulePath.data(), (DWORD)modulePath.capacity());
                    
                    if (numPathCharacters > 0)
                    {
                        internal::LogInfo(__FUNCTION__, (std::wstring(L"Module Path: ") + modulePath).c_str());

                        std::wstring processName = (std::filesystem::path(modulePath).filename()).c_str();
                        internal::LogInfo(__FUNCTION__, (std::wstring(L"Process Name: ") + processName).c_str());

                        m_sessionTracker->AddClientSession(m_id, m_name.c_str(), clientProcessId, processName.c_str());
                    }
                    else
                    {
                        // couldn't get the process name
                        internal::LogGeneralError(__FUNCTION__, L"Unable to get current process name.");

                        return false;
                    }
                }
                else
                {
                    internal::LogGeneralError(__FUNCTION__, L"Error activating IMidiSessionTracker.");

                    return false;
                }

            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Error starting session. Service abstraction is nullptr.");

                return false;
            }

        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception starting session. Service may be unavailable.", ex);

            return false;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception starting session.");

            return false;
        }

        return true;
    }

    //_Use_decl_annotations_
    //winrt::hstring MidiSession::NormalizeDeviceId(const winrt::hstring& endpointDeviceId)
    //{
    //    if (endpointDeviceId.empty()) return endpointDeviceId;

    //    return internal::ToUpperTrimmedHStringCopy(endpointDeviceId);
    //}




    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::CreateEndpointConnection(
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


    // TODO: Wrap up all this JSON crud in another function 

    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::CreateVirtualDeviceAndConnection(
        midi2::MidiVirtualEndpointDeviceDefinition const& deviceDefinition
    ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        winrt::hstring endpointDeviceId{};


        // create the json for creating the endpoint


        // todo: grab this from a constant
        winrt::hstring virtualDeviceAbstractionId = L"{8FEAAD91-70E1-4A19-997A-377720A719C1}";


        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject wrapperObject;
        json::JsonObject abstractionObject;
        json::JsonObject endpointCreationObject;
        json::JsonArray createVirtualDevicesArray;
        json::JsonObject endpointDefinitionObject;

        internal::LogInfo(__FUNCTION__, L"Creating association GUID");

        // Create association guid
        auto associationGuid = foundation::GuidHelper::CreateNewGuid();

        // set all of our properties.

        internal::LogInfo(__FUNCTION__, L"Setting json properties");

        internal::JsonSetGuidProperty(
            endpointDefinitionObject, 
            MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY, 
            associationGuid);

       
        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            json::JsonValue::CreateStringValue(deviceDefinition.EndpointProductInstanceId().c_str()));

        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            json::JsonValue::CreateStringValue(deviceDefinition.EndpointName().c_str()));

        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            json::JsonValue::CreateStringValue(deviceDefinition.TransportSuppliedDescription().c_str()));
       

        // TODO: Other props that have to be set at the service level and not in-protocol

        internal::LogInfo(__FUNCTION__, L"Appending endpoint definition");
        internal::LogInfo(__FUNCTION__, endpointDefinitionObject.Stringify().c_str());

        createVirtualDevicesArray.Append(endpointDefinitionObject);

        internal::LogInfo(__FUNCTION__, L"Adding virtual devices array to abstraction object");

        abstractionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY,
            createVirtualDevicesArray);

        internal::LogInfo(__FUNCTION__, L"Setting named value for virtual abstraction id");

        endpointCreationObject.SetNamedValue(virtualDeviceAbstractionId, abstractionObject);


        // wrap it all in the Transport GUID so it's the same format used in the configuration file
        wrapperObject.SetNamedValue(virtualDeviceAbstractionId, abstractionObject);

        topLevelTransportPluginSettingsObject.SetNamedValue(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT, wrapperObject);


        // this is the format we're sending up
        // 
        //"endpointTransportPluginSettings":
        //{
        //    "{8FEAAD91-70E1-4A19-997A-377720A719C1}":
        //    {
        //       "create":
        //       [
        //           {
        //              ... properties ...
        //           }
        //       ]
        //    }
        //}
        //

        // send it up

        auto iid = __uuidof(IMidiAbstractionConfigurationManager);

        winrt::com_ptr<IMidiAbstractionConfigurationManager> configManager;

        auto activateConfigManagerResult = m_serviceAbstraction->Activate(iid, (void**)&configManager);

        internal::LogInfo(__FUNCTION__, L"Config manager activate call completed");


        if (FAILED(activateConfigManagerResult) || configManager == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to create device. Config manager is null.");

            return nullptr;
        }

        internal::LogInfo(__FUNCTION__, L"Config manager activate call SUCCESS");

        auto initializeResult = configManager->Initialize(internal::StringToGuid(virtualDeviceAbstractionId.c_str()), nullptr, nullptr);


        if (FAILED(initializeResult))
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to initialize config manager");

            return nullptr;
        }


        internal::LogInfo(__FUNCTION__, L"Config manager initialize call SUCCESS");

        CComBSTR response;
        response.Empty();

        internal::LogInfo(__FUNCTION__, L"Sending json to UpdateConfiguration");
        internal::LogInfo(__FUNCTION__, topLevelTransportPluginSettingsObject.Stringify().c_str());

        auto configUpdateResult = configManager->UpdateConfiguration(topLevelTransportPluginSettingsObject.Stringify().c_str(), false, &response);

        internal::LogInfo(__FUNCTION__, L"UpdateConfiguration returned");
        internal::LogInfo(__FUNCTION__, response);

        if (FAILED(configUpdateResult))
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to configure endpoint");

            return nullptr;
        }

        internal::LogInfo(__FUNCTION__, L"configManager->UpdateConfiguration success");

        json::JsonObject responseObject;

        if (!internal::JsonObjectFromBSTR(&response, responseObject))
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to read json response object from virtual device creation");

            return nullptr;
        }

        internal::LogInfo(__FUNCTION__, L"JsonObjectFromBSTR success");

        // check for actual success
        auto successResult = responseObject.GetNamedBoolean(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, false);

        if (successResult)
        {
            internal::LogInfo(__FUNCTION__, L"JSON payload indicates success");

            auto responseArray = responseObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY, nullptr);

            if (responseArray != nullptr && responseArray.Size() == 0)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unexpected empty response array");

                return nullptr;
            }

            // get the id. We should have only one item in the response array here so we'll just grab the first

            auto firstObject = responseArray.GetObjectAt(0);

            endpointDeviceId = firstObject.GetNamedString(
                MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY,
                L"");

            internal::LogInfo(__FUNCTION__, endpointDeviceId.c_str());
        }
        else
        {
            internal::LogGeneralError(__FUNCTION__, L"Virtual device creation failed (payload has false success value)");

            return nullptr;
        }

        internal::LogInfo(__FUNCTION__, L"Virtual device creation worked.");

        // Creating the device succeeded, so create the connection
        auto connection = CreateEndpointConnection(endpointDeviceId, false, nullptr);

        internal::LogInfo(__FUNCTION__, L"Created endpoint connection");

        if (connection)
        {
            auto virtualDevice = winrt::make_self<implementation::MidiVirtualEndpointDevice>();

            virtualDevice->InternalSetDeviceDefinition(deviceDefinition);
            virtualDevice->IsEnabled(true);

            // add the listener
            connection.AddMessageProcessingPlugin(*virtualDevice);
        }
        else
        {
            internal::LogGeneralError(__FUNCTION__, L"Could not add MidiVirtualEndpointDevice because connection was null");
        }

        return connection;
    }


    _Use_decl_annotations_
    bool MidiSession::UpdateName(winrt::hstring const& newName) noexcept
    {
        UNREFERENCED_PARAMETER(newName);

        internal::LogInfo(__FUNCTION__, L"Enter");

        // TODO: Implement UpdateName


        return false;

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
        internal::LogInfo(__FUNCTION__, L"Closing session");

        if (!m_isOpen) return;

        try
        {
            if (m_serviceAbstraction != nullptr)
            {
                // TODO: Call any cleanup method on the service
                for (auto connection : m_connections)
                {
                    // close the one connection
                    //connection.Value().as<foundation::IClosable>().Close();

                    auto c = winrt::get_self<implementation::MidiEndpointConnection>(connection.Value());
                    c->InternalClose();
                }

                m_serviceAbstraction = nullptr;
            }

            if (m_sessionTracker != nullptr)
            {
                m_sessionTracker->RemoveClientSession(m_id);

                m_sessionTracker = nullptr;
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
