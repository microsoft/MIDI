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

    // full instance Id including the interface guid, exactly 
    // as seen using Windows::Devices::Enumeration
    std::wstring GetConfigurationForEndpoint(
        _In_ std::wstring instanceId) const noexcept;

    HRESULT Cleanup() noexcept;

private:
    std::wstring GetCurrentConfigurationFileName() noexcept;

    winrt::Windows::Data::Json::JsonObject m_jsonObject{ nullptr };

};



// this could be moved to someplace better

inline std::wstring GuidToString(_In_ GUID guid)
{
    LPOLESTR str;
    StringFromCLSID(guid, &str);

    // TODO: Is this copying or acquiring?
    std::wstring guidString{ str };

    ::CoTaskMemFree(str);

    return guidString;
}
