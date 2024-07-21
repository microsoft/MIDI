// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once



class CMidi2VirtualPatchBayRoutingSource :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:
    STDMETHOD(Initialize)(_In_ LPCWSTR endpointMatchJson);

    STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT length, _In_ LONGLONG position, _In_ LONGLONG context);

    STDMETHOD(AddIncludedGroups)(_In_ std::vector<uint8_t> groupIndexes);
    STDMETHOD(SetEndpointBiDi)(_In_ LPCWSTR resolvedEndpointDeviceInterfaceId, _In_ IMidiBiDi* sourceEndpointBiDi);

    STDMETHOD(Cleanup)();

private:
    std::wstring m_matchJson{ };
    std::wstring m_resolvedEndpointDeviceInterfaceId{ };

    std::vector<uint8_t> m_includedGroupIndexes{ };

    wil::com_ptr<IMidiBiDi> m_sourceEndpointBiDi{ nullptr };

};

