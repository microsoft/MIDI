#include "stdafx.h"

#include "MidiConfigurationManager.h"

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

std::vector<GUID> CMidiConfigurationManager::GetEnabledAbstractionLayers() const noexcept
{
    //OutputDebugString(L"GetEnabledAbstractionLayers");

    std::vector<GUID> availableAbstractionLayers;

    // the diagnostics abstraction is always instantiated. We don't want to take the
    // chance someone removes this from the registry, so it's hard-coded here. It's
    // needed for support, tools, diagnostics, etc.
    availableAbstractionLayers.push_back(__uuidof(Midi2DiagnosticsAbstraction));

    try
    {
        auto rootKey = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY, wil::reg::key_access::read);

        //OutputDebugString(L"Root Key Opened");

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

        //OutputDebugString(L"Queried Info Key");


        if (cSubKeys)
        {
            for (i = 0; i < cSubKeys; i++)
            {
                //OutputDebugString(L"Reading Sub Key");

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
                    //OutputDebugString(L"Sub Key Read");

                    DWORD abstractionEnabled = 1;

                    // Check to see if that sub key has an Enabled value.

                    std::wstring keyPath = MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY;
                    keyPath += L"\\";
                    keyPath += achKey;

                    //OutputDebugString(L"Opening Key");

                    auto pluginKey = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, keyPath.c_str(), wil::reg::key_access::read);

                    try
                    {
                        //OutputDebugString(L"Getting Enabled Value");

                        // is the abstraction layer enabled? > 0 or missing Enabled value mean it is
                        abstractionEnabled = wil::reg::get_value<DWORD>(pluginKey.get(), MIDI_PLUGIN_ENABLED_REG_VALUE);
                    }
                    catch (...)
                    {
                        // value doesn't exist, so we default to enabled
                        abstractionEnabled = (DWORD)1;
                    }

                    // we only proceed with this abstraction entry if it's enabled
                    if (abstractionEnabled > 0)
                    {
                        try
                        {
                            //OutputDebugString(L"Getting CLSID Value");

                            GUID abstractionLayerGuid;

                            auto clsidString = wil::reg::get_value<std::wstring>(pluginKey.get(), MIDI_PLUGIN_CLSID_REG_VALUE);

                            auto result = CLSIDFromString((LPCTSTR)clsidString.c_str(), (LPGUID)&abstractionLayerGuid);

                            LOG_IF_FAILED(result);

                            if (result == NOERROR)
                            {
                                //OutputDebugString(L"Adding CLSID to Vector");

                                // Add to the vector
                                availableAbstractionLayers.push_back(abstractionLayerGuid);
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
    return availableAbstractionLayers;
}



HRESULT CMidiConfigurationManager::Initialize()
{
    // load the current configuration

    return S_OK;
}


_Use_decl_annotations_
std::wstring CMidiConfigurationManager::GetConfigurationForTransportAbstraction(GUID /*abstractionGuid*/) const noexcept
{
    std::wstring config{};

    // TODO

    return config;
}

_Use_decl_annotations_
std::wstring CMidiConfigurationManager::GetConfigurationForEndpointProcessingAbstraction(GUID /*abstractionGuid*/) const noexcept
{
    std::wstring config{};

    // TODO

    return config;
}

_Use_decl_annotations_
std::wstring CMidiConfigurationManager::GetConfigurationForEndpoint(PCWSTR /*instanceId*/) const noexcept
{
    std::wstring config{};

    // TODO

    return config;
}



HRESULT CMidiConfigurationManager::Cleanup() noexcept
{
    // TODO

    return S_OK;
}