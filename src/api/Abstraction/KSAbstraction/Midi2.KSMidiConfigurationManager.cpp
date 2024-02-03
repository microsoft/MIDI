// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"


_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::Initialize(
    GUID abstractionGuid,
    IUnknown* midiDeviceManager
)
{
    UNREFERENCED_PARAMETER(abstractionGuid);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    return S_OK;
}







_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::UpdateConfiguration(LPCWSTR configurationJson, BSTR* response)
{
    OutputDebugString(L"\n" __FUNCTION__);

    UNREFERENCED_PARAMETER(configurationJson);


    // temp. Also, client needs to free this.
    *response = SysAllocString(L"OK");

    


    return S_OK;

    //try
    //{
    //    // if we have no configuration, that's ok
    //    if (m_jsonObject == nullptr) return S_OK;


    //    std::vector<DEVPROPERTY> endpointProperties;

    //    // for now, we only support lookup by the deviceInterfaceId, so this code is easier

    //    winrt::hstring endpointSettingsKey = winrt::to_hstring(MIDI_CONFIG_JSON_ENDPOINT_IDENTIFIER_SWD) + deviceInterfaceId;

    //    //OutputDebugString(L" Key: ");
    //    //OutputDebugString(endpointSettingsKey.c_str());
    //    //OutputDebugString(L"\n");


    //    if (m_jsonObject.HasKey(endpointSettingsKey))
    //    {
    //        json::JsonObject endpointSettings = m_jsonObject.GetNamedObject(endpointSettingsKey);

    //        // Get the user-specified endpoint name
    //        if (endpointSettings != nullptr && endpointSettings.HasKey(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_NAME_PROPERTY_KEY))
    //        {
    //            auto name = endpointSettings.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_NAME_PROPERTY_KEY);

    //            endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
    //                    DEVPROP_TYPE_STRING, static_cast<ULONG>((name.size() + 1) * sizeof(WCHAR)), (PVOID)name.c_str() });
    //        }
    //        else
    //        {
    //            // delete any existing property value, because it is no longer in the config
    //            endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
    //                    DEVPROP_TYPE_EMPTY, 0, nullptr });
    //        }


    //        // Get the user-specified endpoint description
    //        if (endpointSettings != nullptr && endpointSettings.HasKey(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_DESCRIPTION_PROPERTY_KEY))
    //        {
    //            auto description = endpointSettings.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_DESCRIPTION_PROPERTY_KEY);

    //            endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
    //                    DEVPROP_TYPE_STRING, static_cast<ULONG>((description.size() + 1) * sizeof(WCHAR)), (PVOID)description.c_str() });
    //        }
    //        else
    //        {
    //            // delete any existing property value, because it is no longer in the config
    //            endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
    //                    DEVPROP_TYPE_EMPTY, 0, nullptr });
    //        }

    //        // Get the user-specified multiclient override
    //        if (endpointSettings != nullptr && endpointSettings.HasKey(MIDI_CONFIG_JSON_ENDPOINT_FORCE_SINGLE_CLIENT_PROPERTY_KEY))
    //        {
    //            auto forceSingleClient = endpointSettings.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_FORCE_SINGLE_CLIENT_PROPERTY_KEY);

    //            if (forceSingleClient)
    //            {
    //                DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    //                endpointProperties.push_back({ {PKEY_MIDI_SupportsMulticlient, DEVPROP_STORE_SYSTEM, nullptr},
    //                        DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropFalse)), (PVOID)&devPropFalse });
    //            }
    //        }
    //        else
    //        {
    //            // this property was an override, so it should have been set elsewhere earlier
    //        }

    //        // apply supported property changes.
    //        if (endpointProperties.size() > 0)
    //        {
    //            return m_MidiDeviceManager->UpdateEndpointProperties(
    //                (LPCWSTR)deviceInterfaceId.c_str(),
    //                (ULONG)endpointProperties.size(),
    //                (PVOID)endpointProperties.data());
    //        }
    //    }

    //    return S_OK;
    //}
    //catch (...)
    //{
    //    return E_FAIL;
    //}
}



_Use_decl_annotations_
HRESULT
CMidi2KSMidiConfigurationManager::Cleanup()
{
    return S_OK;
}