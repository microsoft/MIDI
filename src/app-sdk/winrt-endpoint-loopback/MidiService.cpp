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
