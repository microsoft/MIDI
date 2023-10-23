// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidiConfigurationManager
{
public:

    CMidiConfigurationManager() = default;
    ~CMidiConfigurationManager() {}

    HRESULT Initialize();

//    HRESULT LoadCurrentConfiguration();

    std::vector<GUID> GetEnabledTransportAbstractionLayers() const noexcept;
    std::vector<GUID> GetEnabledEndpointProcessingAbstractionLayers() const noexcept;

    // takes a string because that's what we convert to anyway 
    // expects the guid in the form with braces
    std::wstring GetConfigurationForTransportAbstraction(
        _In_ GUID abstractionGuid) const noexcept;

    // takes a string because that's what we convert to anyway 
    // expects the guid in the form with braces
    std::wstring GetConfigurationForEndpointProcessingAbstraction(
        _In_ GUID abstractionGuid) const noexcept;

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




