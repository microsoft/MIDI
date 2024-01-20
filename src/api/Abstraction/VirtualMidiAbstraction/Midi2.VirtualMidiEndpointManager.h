// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


class CMidi2VirtualMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IUnknown*, _In_ IUnknown*, _In_ LPCWSTR));
    STDMETHOD(Cleanup)();

    //STDMETHOD(ApplyConfiguration(
    //    _In_ LPCWSTR configurationJson,
    //    _Out_ LPWSTR resultJson
    //));


private:
    GUID m_ContainerId{};
    GUID m_TransportAbstractionId{};

    HRESULT CreateDeviceSideEndpoint(
        _In_ GUID const VirtualEndpointAssociationGuid,
        _Out_ std::wstring InstanceId
    );

    HRESULT CreateClientVisibleEndpoint(
        _In_ GUID const VirtualEndpointAssociationGuid,
        _In_ std::wstring DeviceSideInstanceId,
        _In_ std::wstring const Name,
        _In_ std::wstring const LargeImagePath,
        _In_ std::wstring const SmallImagePath,
        _In_ std::wstring const Description,
        _Out_ std::wstring InstanceId
    );

    HRESULT ApplyJson(_In_ json::JsonObject jsonObject);

    HRESULT DeleteEndpointPair(
        _In_ GUID const VirtualEndpointAssociationGuid
    );

    HRESULT CreateConfiguredDeviceEndpoints(_In_ std::wstring ConfigurationJson);
    
    HRESULT CreateParentDevice();

    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;


    //json::JsonObject m_JsonObject{ nullptr };
};
