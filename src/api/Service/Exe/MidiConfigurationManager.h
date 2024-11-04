// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
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

    HRESULT LoadCurrentConfigurationFile();


    // given a JSON object with create/update/remove child keys, 
    // will return the contents of any create key
    STDMETHOD(GetTransportCreateActionEntry)(
        _In_ LPCWSTR sourceTransportJson,
        _Out_ LPWSTR* responseJson
        );

    // given a JSON object with create/update/remove child keys, 
    // will return the contents of any update key
    STDMETHOD(GetTransportUpdateActionEntry)(
        _In_ LPCWSTR sourceTransportJson,
        _Out_ LPWSTR* responseJson
        );

    // given a JSON object with create/update/remove child keys, 
    // will return the contents of any remove key
    STDMETHOD(GetTransportRemoveActionEntry)(
        _In_ LPCWSTR sourceTransportJson,
        _Out_ LPWSTR* responseJson
        );

    // given the contents of a create/update/remove object, will return the object which matches the criteria
    // provided in the searchKeyValuePairsJson json object
    STDMETHOD(GetMatchingEndpointEntry)(
        _In_ LPCWSTR sourceActionObjectJson,
        _In_ LPCWSTR searchKeyValuePairsJson,
        _Out_ LPWSTR* responseJson
        );

    // Uses the internal cache of config file entries and returns any matching update json for the 
    // specified transport. This is needed for transports that create devices after the initial
    // configuration has been read.
    STDMETHOD(GetCachedEndpointUpdateEntry)(
        _In_ GUID transportId,
        _In_ LPCWSTR searchKeyValuePairsJson,
        _Out_ LPWSTR* responseJson
    );


    // TODO: the endpoint lookup table should be maintained in memory here, and can be updated/reloaded
    // We don't want to pass a gigantic string back and forth each time, so need to just keep it live

    HRESULT Shutdown() noexcept;



    std::vector<GUID> GetEnabledTransports() const noexcept;
    std::vector<GUID> GetEnabledEndpointProcessingTransforms() const noexcept;

    std::vector<TRANSPORTMETADATA> GetAllEnabledTransportMetadata() const noexcept;



    std::wstring GetSavedConfigurationForTransport(
        _In_ GUID transportGuid) const noexcept;

    std::wstring GetSavedConfigurationForEndpointProcessingTransform(
        _In_ GUID transportGuid) const noexcept;


    std::map<GUID, std::wstring, GUIDCompare> GetTransportSettingsFromJsonString(
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




