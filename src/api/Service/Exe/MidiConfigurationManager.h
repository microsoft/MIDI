// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidiConfigurationManager : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiServiceConfigurationManagerInterface>
{
public:

    CMidiConfigurationManager() = default;
    ~CMidiConfigurationManager() {}

    HRESULT Initialize();





    // given a JSON object with create/update/remove child keys, 
    // will return the contents of any create key
    STDMETHOD(GetAbstractionCreateActionJsonObject)(
        _In_ LPCWSTR sourceAbstractionJson,
        _Out_ BSTR* responseJson
        );

    // given a JSON object with create/update/remove child keys, 
    // will return the contents of any update key
    STDMETHOD(GetAbstractionUpdateActionJsonObject)(
        _In_ LPCWSTR sourceAbstractionJson,
        _Out_ BSTR* responseJson
        );

    // given a JSON object with create/update/remove child keys, 
    // will return the contents of any remove key
    STDMETHOD(GetAbstractionRemoveActionJsonObject)(
        _In_ LPCWSTR sourceAbstractionJson,
        _Out_ BSTR* responseJson
        );

    // given the contents of a create/update/remove object, will return the object which matches the criteria
    // provided in the searchKeyValuePairsJson json object
    STDMETHOD(GetAbstractionMatchingEndpointJsonObject)(
        _In_ LPCWSTR sourceActionObjectJson,
        _In_ LPCWSTR searchKeyValuePairsJson,
        _Out_ BSTR* responseJson
        );



    // TODO: the endpoint lookup table should be maintained in memory here, and can be updated/reloaded
    // We don't want to pass a gigantic string back and forth each time, so need to just keep it live





    HRESULT Cleanup() noexcept;



    std::vector<GUID> GetEnabledTransportAbstractionLayers() const noexcept;
    std::vector<GUID> GetEnabledEndpointProcessingTransforms() const noexcept;

    std::wstring GetSavedConfigurationForTransportAbstraction(
        _In_ GUID abstractionGuid) const noexcept;

    std::wstring GetSavedConfigurationForEndpointProcessingTransform(
        _In_ GUID abstractionGuid) const noexcept;


    std::map<GUID, std::wstring, GUIDCompare> GetTransportAbstractionSettingsFromJsonString(
        _In_ std::wstring json) const noexcept;


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




