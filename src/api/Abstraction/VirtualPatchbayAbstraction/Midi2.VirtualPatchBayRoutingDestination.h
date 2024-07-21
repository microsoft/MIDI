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

    std::wstring MatchJson{ };
    std::wstring ResolvedEndpointDeviceInterfaceId{ };

    wil::com_ptr_nothrow<IMidiBiDi> EndpointBiDi{ nullptr };

    std::map<uint8_t, uint8_t> GroupTransformMap{ };    // incoming group, outgoing group

};
