// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidiConfigurationManager
{
public:

    CMidiConfigurationManager() = default;
    ~CMidiConfigurationManager() {}

    HRESULT Initialize();

//    HRESULT LoadCurrentConfiguration();

    std::vector<GUID> GetEnabledAbstractionLayers() const noexcept;

    std::wstring GetConfigurationForTransportAbstraction(
        _In_ GUID abstractionGuid) const noexcept;

    std::wstring GetConfigurationForEndpointProcessingAbstraction(
        _In_ GUID abstractionGuid) const noexcept;

    std::wstring GetConfigurationForEndpoint(
        _In_ PCWSTR instanceId) const noexcept;

    HRESULT Cleanup() noexcept;

private:


};
