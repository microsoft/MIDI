// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidiConfigurationManager
{
public:

    CMidiConfigurationManager() = default;
    ~CMidiConfigurationManager() {}

    HRESULT Initialize();

//    HRESULT LoadCurrentConfiguration();

    std::vector<GUID> GetEnabledTransportAbstractionLayers() const noexcept;
    std::vector<GUID> GetEnabledEndpointProcessingTransforms() const noexcept;

    std::wstring GetSavedConfigurationForTransportAbstraction(
        _In_ GUID abstractionGuid) const noexcept;

    std::wstring GetSavedConfigurationForEndpointProcessingTransform(
        _In_ GUID abstractionGuid) const noexcept;


    std::map<GUID, std::wstring, GUIDCompare> GetTransportAbstractionSettingsFromJsonString(
        _In_ std::wstring json) const noexcept;

    HRESULT Cleanup() noexcept;

private:
    std::wstring GetCurrentConfigurationFileName() noexcept;

    json::JsonObject m_jsonObject{ nullptr };


    // this works as long as the path is not user-specific. If we decide to store in 
    // user's local app settings or something, then we need to impersonate before
    // calling this function
    inline std::wstring ExpandPath(_In_ std::wstring sourcePath) const
    {
        wchar_t expanded[MAX_PATH + 1]{ 0 };

        /*auto numChars = */ ExpandEnvironmentStrings(sourcePath.c_str(), (LPWSTR)&expanded, MAX_PATH + 1);

        return std::wstring(expanded);
    }



};




