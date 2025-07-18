// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


//
// TODO: Refactor this code to use the json_helpers.h functions and defines
//


#include "stdafx.h"

//#include "MidiConfigurationManager.h"

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383


// TODO: Refactor these two methods and abstract out the registry code. Do this once wil adds the enumeration helpers to the NuGet
std::vector<GUID> CMidiConfigurationManager::GetEnabledTransports() const noexcept
{
    std::vector<GUID> availableTransportLayers;

    // the diagnostics transport is always instantiated. We don't want to take the
    // chance someone removes this from the registry, so it's hard-coded here. It's
    // needed for support, tools, diagnostics, etc.
    availableTransportLayers.push_back(__uuidof(Midi2DiagnosticsTransport));

    try
    {
        auto rootKey = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY, wil::reg::key_access::read);

        // current wil NuGet doesn't include the new registry key/value enumeration, so have to do this the hard way
        // https://github.com/microsoft/wil/commit/a6c9f2d8520e830b04c47c3c395bac428696cc0d

        TCHAR    achKey[MAX_KEY_LENGTH];        // buffer for subkey name
        DWORD    cbName;                        // size of name string 
        TCHAR    achClass[MAX_PATH] = TEXT(""); // buffer for class name 
        DWORD    cchClassName = MAX_PATH;       // size of class string 
        DWORD    cSubKeys = 0;                  // number of subkeys 
        DWORD    cbMaxSubKey;                   // longest subkey size 
        DWORD    cchMaxClass;                   // longest class string 
        DWORD    cValues;                       // number of values for key 
        DWORD    cchMaxValue;                   // longest value name 
        DWORD    cbMaxValueData;                // longest value data 
        DWORD    cbSecurityDescriptor;          // size of security descriptor 
        FILETIME ftLastWriteTime;               // last write time 

        DWORD i, retCode;

        //TCHAR  achValue[MAX_VALUE_NAME];
        //DWORD cchValue = MAX_VALUE_NAME;

        // Get the class name and the value count. 
        retCode = RegQueryInfoKey(
            rootKey.get(),           // key handle 
            achClass,                // buffer for class name 
            &cchClassName,           // size of class string 
            NULL,                    // reserved 
            &cSubKeys,               // number of subkeys 
            &cbMaxSubKey,            // longest subkey size 
            &cchMaxClass,            // longest class string 
            &cValues,                // number of values for this key 
            &cchMaxValue,            // longest value name 
            &cbMaxValueData,         // longest value data 
            &cbSecurityDescriptor,   // security descriptor 
            &ftLastWriteTime);       // last write time 

        // Enumerate the subkeys, until RegEnumKeyEx fails.

        if (cSubKeys)
        {
            for (i = 0; i < cSubKeys; i++)
            {
                cbName = MAX_KEY_LENGTH;

                retCode = RegEnumKeyEx(
                    rootKey.get(), 
                    i,
                    achKey,
                    &cbName,
                    NULL,
                    NULL,
                    NULL,
                    &ftLastWriteTime);

                if (retCode == ERROR_SUCCESS)
                {
                    DWORD transportEnabled = 1;

                    // Check to see if that sub key has an Enabled value.

                    std::wstring keyPath = MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY;
                    keyPath += L"\\";
                    keyPath += achKey;

                    auto pluginKey = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, keyPath.c_str(), wil::reg::key_access::read);

                    try
                    {
                        // is the transport layer enabled? > 0 or missing Enabled value mean it is
                        transportEnabled = wil::reg::get_value<DWORD>(pluginKey.get(), MIDI_PLUGIN_ENABLED_REG_VALUE);
                    }
                    catch (...)
                    {
                        // value doesn't exist, so we default to enabled
                        transportEnabled = (DWORD)1;
                    }

                    // we only proceed with this transport entry if it's enabled
                    if (transportEnabled > 0)
                    {
                        try
                        {
                            GUID transportLayerGuid;

                            auto clsidString = wil::reg::get_value<std::wstring>(pluginKey.get(), MIDI_PLUGIN_CLSID_REG_VALUE);

                            auto result = CLSIDFromString((LPCTSTR)clsidString.c_str(), (LPGUID)&transportLayerGuid);

                            LOG_IF_FAILED(result);

                            if (result == NOERROR)
                            {
                                // verify that there are no existing entries for this class id

                                if (std::find(availableTransportLayers.begin(), availableTransportLayers.end(), transportLayerGuid) == availableTransportLayers.end())
                                {
                                    // Doesn't already exist, so add to the vector
                                    availableTransportLayers.push_back(transportLayerGuid);
                                }
                                else
                                {
                                    TraceLoggingWrite(
                                        MidiSrvTelemetryProvider::Provider(),
                                        MIDI_TRACE_EVENT_WARNING,
                                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                                        TraceLoggingWideString(L"Duplicate transport GUID in registry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                        TraceLoggingGuid(transportLayerGuid, "id"),
                                        TraceLoggingPointer(this, "this")
                                    );
                                }
                            }

                        }
                        CATCH_LOG()
                    }
                }
            }
        }

    }
    CATCH_LOG()

    // return a copy
    return availableTransportLayers;
}

// TODO: Refactor these two methods and abstract out the registry code. Do this once wil adds the enumeration helpers to the NuGet
std::vector<GUID> CMidiConfigurationManager::GetEnabledEndpointProcessingTransforms() const noexcept
{
    std::vector<GUID> availableTransforms;

    // TODO: We'll want to add the required transforms here, like the translation ones
   // availableTransforms.push_back(__uuidof(Midi2BS2UMPTransform));
   // availableTransforms.push_back(__uuidof(Midi2UMP2BSTransform));
   // availableTransforms.push_back(__uuidof(Midi2ProtocolDownscalerTransform));
   // availableTransforms.push_back(__uuidof(Midi2SchedulerTransform));

    try
    {
        auto rootKey = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, MIDI_ROOT_ENDPOINT_PROCESSING_PLUGINS_REG_KEY, wil::reg::key_access::read);

        // current wil NuGet doesn't include the new registry key/value enumeration, so have to do this the hard way
        // https://github.com/microsoft/wil/commit/a6c9f2d8520e830b04c47c3c395bac428696cc0d

        TCHAR    achKey[MAX_KEY_LENGTH];        // buffer for subkey name
        DWORD    cbName;                        // size of name string 
        TCHAR    achClass[MAX_PATH] = TEXT(""); // buffer for class name 
        DWORD    cchClassName = MAX_PATH;       // size of class string 
        DWORD    cSubKeys = 0;                  // number of subkeys 
        DWORD    cbMaxSubKey;                   // longest subkey size 
        DWORD    cchMaxClass;                   // longest class string 
        DWORD    cValues;                       // number of values for key 
        DWORD    cchMaxValue;                   // longest value name 
        DWORD    cbMaxValueData;                // longest value data 
        DWORD    cbSecurityDescriptor;          // size of security descriptor 
        FILETIME ftLastWriteTime;               // last write time 

        DWORD i, retCode;

        //TCHAR  achValue[MAX_VALUE_NAME];
        //DWORD cchValue = MAX_VALUE_NAME;

        // Get the class name and the value count. 
        retCode = RegQueryInfoKey(
            rootKey.get(),           // key handle 
            achClass,                // buffer for class name 
            &cchClassName,           // size of class string 
            NULL,                    // reserved 
            &cSubKeys,               // number of subkeys 
            &cbMaxSubKey,            // longest subkey size 
            &cchMaxClass,            // longest class string 
            &cValues,                // number of values for this key 
            &cchMaxValue,            // longest value name 
            &cbMaxValueData,         // longest value data 
            &cbSecurityDescriptor,   // security descriptor 
            &ftLastWriteTime);       // last write time 

        // Enumerate the subkeys, until RegEnumKeyEx fails.

        if (cSubKeys)
        {
            for (i = 0; i < cSubKeys; i++)
            {
                cbName = MAX_KEY_LENGTH;

                retCode = RegEnumKeyEx(
                    rootKey.get(),
                    i,
                    achKey,
                    &cbName,
                    NULL,
                    NULL,
                    NULL,
                    &ftLastWriteTime);

                if (retCode == ERROR_SUCCESS)
                {
                    DWORD transformEnabled = 1;

                    // Check to see if that sub key has an Enabled value.

                    std::wstring keyPath = MIDI_ROOT_ENDPOINT_PROCESSING_PLUGINS_REG_KEY;
                    keyPath += L"\\";
                    keyPath += achKey;

                    auto pluginKey = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, keyPath.c_str(), wil::reg::key_access::read);

                    try
                    {
                        // is the transform enabled? > 0 or missing Enabled value mean it is
                        transformEnabled = wil::reg::get_value<DWORD>(pluginKey.get(), MIDI_PLUGIN_ENABLED_REG_VALUE);
                    }
                    catch (...)
                    {
                        // value doesn't exist, so we default to enabled
                        transformEnabled = (DWORD)1;
                    }

                    // we only proceed with this transport entry if it's enabled
                    if (transformEnabled > 0)
                    {
                        try
                        {
                            GUID transformGuid;

                            auto clsidString = wil::reg::get_value<std::wstring>(pluginKey.get(), MIDI_PLUGIN_CLSID_REG_VALUE);

                            auto result = CLSIDFromString((LPCTSTR)clsidString.c_str(), (LPGUID)&transformGuid);

                            LOG_IF_FAILED(result);

                            if (result == NOERROR)
                            {
                                // Add to the vector
                                availableTransforms.push_back(transformGuid);
                            }


                            if (std::find(availableTransforms.begin(), availableTransforms.end(), transformGuid) == availableTransforms.end())
                            {
                                // Doesn't already exist, so add to the vector
                                availableTransforms.push_back(transformGuid);
                            }
                            else
                            {
                                TraceLoggingWrite(
                                    MidiSrvTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_WARNING,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                                    TraceLoggingWideString(L"Duplicate transform GUID in registry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingGuid(transformGuid, "id"),
                                    TraceLoggingPointer(this, "this")
                                );
                            }

                        }
                        CATCH_LOG()
                    }
                }
            }
        }

    }
    CATCH_LOG()

    // return a copy
    return availableTransforms;
}


std::vector<TRANSPORTMETADATA> CMidiConfigurationManager::GetAllEnabledTransportMetadata() const noexcept
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    std::vector<TRANSPORTMETADATA> results{};

    auto transportIdList = GetEnabledTransports();

    // for each item in the list, activate the MidiServiceTransportPlugin and get the metadata

    for (auto const& transportId : transportIdList)
    {
        wil::com_ptr_nothrow<IMidiTransport> midiTransport;
        wil::com_ptr_nothrow<IMidiServiceTransportPluginMetadataProvider> plugin;

        // Do not load any transports which are untrusted, unless in developer mode.
        if (SUCCEEDED(internal::IsComponentPermitted(transportId)))
        {
            if (SUCCEEDED(CoCreateInstance(transportId, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport))))
            {
                if (SUCCEEDED(midiTransport->Activate(__uuidof(IMidiServiceTransportPluginMetadataProvider), (void**)&plugin)))
                {
                    plugin->Initialize();

                    TRANSPORTMETADATA metadata;

                    LOG_IF_FAILED(plugin->GetMetadata(&metadata));

                    results.push_back(std::move(metadata));

                    plugin->Shutdown();
                }
                else
                {
                    // log that the interface isn't there, but don't terminate or anything

                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingWideString(L"Unable to activate IMidiServiceTransportPlugin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingGuid(transportId, "transport id"),
                        TraceLoggingPointer(this, "this")
                    );
                }
            }
        }
    }

    return results;
}


// this gets just the file name, not the full path
std::wstring CMidiConfigurationManager::GetCurrentConfigurationFileName() noexcept
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    std::wstring keyPath = MIDI_ROOT_REG_KEY;

    try
    {
        auto val = wil::reg::get_value<std::wstring>(
            HKEY_LOCAL_MACHINE, 
            keyPath.c_str(), 
            MIDI_CONFIG_FILE_REG_VALUE);


        if (!val.empty())
        {
            // strip out any slashes just to prevent hacking this value
            // I couldn't find a built-in function to do this, although I'm sure
            // something reasonable exists
            const std::wstring illegalCharacters = L"><:?*/\\|";
            for (auto iter = val.begin(); iter < val.end(); iter++)
            {
                // in-place sub any illegal characters with a dash
                if (illegalCharacters.find(*iter) != std::wstring::npos)
                    *iter = '-';
            }
        }
        return val;
    }
    catch (...)
    {
        // value is not present in the registry, so just return empty string

        return L"";
    }

}



#pragma push_macro("GetObject")
#undef GetObject

std::map<GUID, std::wstring, GUIDCompare> CMidiConfigurationManager::GetTransportSettingsFromJsonString(
    _In_ std::wstring jsonStringSource) const noexcept
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    //std::map<winrt::guid, std::wstring> transportSettings{};
    std::map<GUID, std::wstring, GUIDCompare> transportSettings{};

    try
    {       
        json::JsonObject jsonObject{ };

        try
        {
            if (!json::JsonObject::TryParse(jsonStringSource, jsonObject))
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"JSON Object parsing failed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(jsonStringSource.c_str(), "Source JSON String")
                );

                // return the empty map
                return transportSettings;
            }
        }
        catch (...)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"JSON Object parsing failed. Exception.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            return transportSettings;
        }


        if (jsonObject == nullptr)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"JSON Object null after parsing", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            // return the empty map
            return transportSettings;
        }


        // we treat this as an object where each transport id is a property
        auto plugins = jsonObject.GetNamedObject(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT, nullptr);

        if (plugins == nullptr)
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No transport plugins node in JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            // return the empty map
            return transportSettings;
        }

        // Iterate through nodes and find each transport transport entry. Parse the GUID. Add to results.

        auto it = plugins.First();

        while (it.HasCurrent())
        {
            std::wstring key = it.Current().Key().c_str();

            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(key.c_str(), "TransportIdGuidString", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            std::wstring transportJson = it.Current().Value().GetObject().Stringify().c_str();

            GUID transportId;

            try
            {
                transportId = internal::StringToGuid(key);
            }
            catch (...)
            {
                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(key.c_str(), "Invalid GUID Property")
                );
            }

            // TODO: Should verify the transportId is for an enabled transport
            // before adding it to the returned map

            transportSettings.insert_or_assign(transportId, transportJson);

            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(transportJson.c_str()),
                TraceLoggingGuid(transportId)
            );

            it.MoveNext();
        }

    }
    CATCH_LOG();

    return transportSettings;
}
#pragma pop_macro("GetObject")



HRESULT
CMidiConfigurationManager::Initialize()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return LoadCurrentConfigurationFile();
}


HRESULT 
CMidiConfigurationManager::LoadCurrentConfigurationFile()
{
    // load the current configuration

    auto fileName = GetCurrentConfigurationFileName();

    // expanding this requires that the service is impersonating the current user.
    // WinRT doesn't support relative paths or unexpanded 
    fileName = ExpandPath(MIDI_CONFIG_FILE_FOLDER) + fileName;

    if (!fileName.empty())
    {
        winrt::hstring fileContents{};

        try
        {
            // try to open the file
            auto file = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(fileName).get();

            // try to read the text from the file
            fileContents = winrt::Windows::Storage::FileIO::ReadTextAsync(file).get();
        }
        catch (...)
        {
            // file does not exist or we can't open it
            // we don't fail if no configuration file, we just don't config anything

            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Missing or inaccessible MIDI configuration file")
            );

            return S_OK;
        }

        if (!fileContents.empty())
        {
            // parse out the JSON.
            // If the JSON is bad, we still run, we just don't config.
            // Config is a bonus, and is certainly not essential :)

            try
            {
                if (!json::JsonObject::TryParse(fileContents, m_jsonObject))
                {
                    TraceLoggingWrite(
                        MidiSrvTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Configuration file JSON parsing failed")
                    );

                    return S_OK;
                }


                // TODO: Cache the settings for each transport in the internal dictionary











            }
            CATCH_LOG()
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Configuration file JSON is empty")
            );

            return S_OK;
        }

    }
    else
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Missing MIDI configuration file")
        );

        return S_OK;
    }

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidiConfigurationManager::GetTransportCreateActionEntry(
    LPCWSTR sourceTransportJson,
    LPWSTR* responseJson
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    UNREFERENCED_PARAMETER(sourceTransportJson);
    UNREFERENCED_PARAMETER(responseJson);



    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidiConfigurationManager::GetTransportUpdateActionEntry(
    LPCWSTR sourceTransportJson,
    LPWSTR* responseJson
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    UNREFERENCED_PARAMETER(sourceTransportJson);
    UNREFERENCED_PARAMETER(responseJson);



    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidiConfigurationManager::GetTransportRemoveActionEntry(
    LPCWSTR sourceTransportJson,
    LPWSTR* responseJson
    )
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    UNREFERENCED_PARAMETER(sourceTransportJson);
    UNREFERENCED_PARAMETER(responseJson);



    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
CMidiConfigurationManager::GetMatchingEndpointEntry(
    LPCWSTR sourceActionObjectJson,
    LPCWSTR searchKeyValuePairsJson,
    LPWSTR* responseJson
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    UNREFERENCED_PARAMETER(sourceActionObjectJson);
    UNREFERENCED_PARAMETER(searchKeyValuePairsJson);
    UNREFERENCED_PARAMETER(responseJson);




    return E_NOTIMPL;
}



// the searchKeyValuePairsJson should look like this. The search json
// is set up so there is an array of objects, each one is a set of 
// keys that must all match. We stop at the first full match we find, 
// so order is important. We use json for this and strings because
// each transport will have its own set of possible keys and values to
// match to decide if an endpoint is the one they're looking for. And,
// frankly, trying to represent this in a COM-friendly way is a mess.
// 
// Note that we always stop at the first match. As far as this function
// is concerned, there will be at most one matching endpoint for any
// given set of keys and values.
// 
// [
//   {
//     "keyname0" : "value0"
//   },
//   {
//     "keyname1" : "value1",
//     "keyname2" : "value2"
//   },
//   {
//     "keyname1" : "value1",
//     "keyname3" : "value3",
//     "keyname4" : "value4"
//   }
// ]
//
_Use_decl_annotations_
HRESULT
CMidiConfigurationManager::GetCachedEndpointUpdateEntry(
    GUID transportId,
    LPCWSTR searchKeyValuePairsJson,
    LPWSTR* responseJson
    )
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    try
    {
        auto jsonSearchKeySets = json::JsonArray::Parse(searchKeyValuePairsJson);
        auto transportKey = internal::GuidToString(transportId);

        if (m_jsonObject != nullptr)
        {
            if (m_jsonObject.HasKey(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT))
            {
                auto plugins = m_jsonObject.GetNamedObject(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT);

                if (plugins.HasKey(transportKey))
                {
                    auto thisPlugin = plugins.GetNamedObject(transportKey);
                    auto updateList = thisPlugin.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, json::JsonArray{});

                    // now, search for property matches. The search json is set up so there is an array of objects, each one
                    // is a set of keys that must all match. We stop at the first full match we find, so order is important

                    if (updateList.Size() > 0)
                    {
                        TraceLoggingWrite(
                            MidiSrvTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Processing update list", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                        );

                        for (auto const& searchkeySet : jsonSearchKeySets)
                        {
                            auto searchkeySetObject = searchkeySet.GetObject();

                            for (auto const& updateItem : updateList)
                            {
                                auto updateItemMatchCriteria = updateItem.GetObject().GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_OBJECT_KEY, nullptr);

                                if (updateItemMatchCriteria != nullptr && updateItemMatchCriteria.Size() > 0)
                                {
                                    bool match = false;

                                    // check key value pair by key value pair
                                    for (auto const& searchKey : searchkeySetObject)
                                    {
                                        if (updateItemMatchCriteria.HasKey(searchKey.Key()))
                                        {
                                            if (updateItemMatchCriteria.GetNamedString(searchKey.Key()) == searchKey.Value().GetString())
                                            {
                                                // if there's more than one key to check, this is only a partial match so far
                                                match = true;
                                            }
                                            else
                                            {
                                                match = false;
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            // we don't have one of the keys, so not a match
                                            match = false;
                                            break;
                                        }

                                    }

                                    if (match)
                                    {
                                        TraceLoggingWrite(
                                            MidiSrvTelemetryProvider::Provider(),
                                            MIDI_TRACE_EVENT_INFO,
                                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                            TraceLoggingPointer(this, "this"),
                                            TraceLoggingWideString(L"Found match. Returning it", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                                        );


                                        // wrap this up so it has the correct form. Updates are always an array inside an object

                                        // {
                                        //   "update":
                                        //   [
                                        //     { the object here }
                                        //   ]
                                        // }
                                        
                                        json::JsonArray updateArray{};
                                        updateArray.Append(updateItem);

                                        json::JsonObject wrapperObject{};
                                        wrapperObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, updateArray);

                                        internal::JsonStringifyObjectToOutParam(wrapperObject.GetObject(), responseJson);

                                        return S_OK;
                                    }

                                }
                            }
                        }
                    }
                }

                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(searchKeyValuePairsJson, "searchKeyValuePairsJson"),
                    TraceLoggingWideString(L"No match found", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                // We couldn't find any matches. This is a soft error, but still needs to be reported as an error
                return E_NOTFOUND;
            }
        }

    }
    catch (...)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(searchKeyValuePairsJson, "searchKeyValuePairsJson"),
            TraceLoggingWideString(L"Exception. Returning E_FAIL", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

    }

    
    return E_FAIL;

}


_Use_decl_annotations_
std::wstring CMidiConfigurationManager::GetSavedConfigurationForTransport(GUID transportId) const noexcept
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(transportId)
    );


    try
    {

        auto key = internal::GuidToString(transportId);

        if (m_jsonObject != nullptr)
        {
            // probably need to normalize these to ignore case.
            if (m_jsonObject.HasKey(winrt::to_hstring(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT)))
            {
                auto plugins = m_jsonObject.GetNamedObject(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT);

                if (plugins.HasKey(key))
                {
                    auto thisPlugin = plugins.GetNamedObject(key);

                    std::wstring jsonString = (std::wstring)thisPlugin.Stringify();

                    return jsonString;
                }
            }
        }
    }
    CATCH_LOG();

    return L"";
}



_Use_decl_annotations_
std::wstring CMidiConfigurationManager::GetSavedConfigurationForEndpointProcessingTransform(GUID transportId) const noexcept
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(transportId)
    );

    
    try
    {
        auto key = internal::GuidToString(transportId);

        if (m_jsonObject != nullptr)
        {
            // probably need to normalize these to ignore case. Not sure if WinRT Json dictionary is case-sensitive
            if (m_jsonObject.HasKey(winrt::to_hstring(MIDI_CONFIG_JSON_ENDPOINT_PROCESSING_PLUGIN_SETTINGS_OBJECT)))
            {
                auto plugins = m_jsonObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_PROCESSING_PLUGIN_SETTINGS_OBJECT);

                if (plugins.HasKey(key))
                {
                    auto thisPlugin = plugins.GetNamedObject(key);

                    std::wstring jsonString = (std::wstring)thisPlugin.Stringify();

                    return jsonString;
                }
            }
        }
    }
    CATCH_LOG();

    return L"";
}



HRESULT CMidiConfigurationManager::Shutdown() noexcept
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );



    return S_OK;
}