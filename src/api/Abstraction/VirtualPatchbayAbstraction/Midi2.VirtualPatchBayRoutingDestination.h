// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once




class CMidi2VirtualPatchBayRoutingDestination :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:
    STDMETHOD(Initialize)(_In_ LPCWSTR endpointMatchJson, _In_ CMidi2VirtualPatchBayRoutingEntry* router);

    // this callback is called from the routing entry. This function is responsible for doing
    // any required group transformation and then 
    STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT length, _In_ LONGLONG position, _In_ LONGLONG context);

    STDMETHOD(SetGroupTransforms)(_In_ std::map<uint8_t, uint8_t> groupTransformMap);
    STDMETHOD(SetEndpointBiDi)(_In_ LPCWSTR resolvedEndpointDeviceInterfaceId, _In_ IMidiBiDi* destinationEndpointBiDi);

    STDMETHOD(Cleanup)();

private:
    CMidi2VirtualPatchBayRoutingEntry* m_router;

    std::wstring m_matchJson{ };
    std::wstring m_resolvedEndpointDeviceInterfaceId{ };

    wil::com_ptr_nothrow<IMidiBiDi> m_endpointBiDi{ nullptr };

    std::map<uint8_t, uint8_t> m_groupTransformMap{ };    // incoming group, outgoing group

};
