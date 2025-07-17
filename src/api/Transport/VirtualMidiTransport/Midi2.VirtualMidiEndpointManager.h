// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


class CMidi2VirtualMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    //STDMETHOD(ApplyConfiguration(
    //    _In_ LPCWSTR configurationJson,
    //    _Out_ LPWSTR resultJson
    //));


    HRESULT CreateDeviceSideEndpoint(
        _Inout_ MidiVirtualDeviceEndpointEntry& entry
    );

    HRESULT CreateClientVisibleEndpoint(
        _Inout_ MidiVirtualDeviceEndpointEntry& entry
    );

    HRESULT NegotiateAndRequestMetadata(_In_ std::wstring endpointInterfaceId);

    HRESULT DeleteClientEndpoint(_In_ std::wstring clientShortInstanceId);

    HRESULT DeleteDeviceEndpoint(_In_ std::wstring deviceShortInstanceId);


    //HRESULT DeleteEndpointPair(
    //    _In_ GUID const VirtualEndpointAssociationGuid
    //);

private:
    GUID m_ContainerId{};

    std::wstring m_parentDeviceId{};


//    HRESULT CreateConfiguredDeviceEndpoints(_In_ std::wstring ConfigurationJson);
    
    HRESULT CreateParentDevice();

    wil::com_ptr_nothrow<IMidiDeviceManager> m_MidiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_MidiProtocolManager;

    //json::JsonObject m_JsonObject{ nullptr };
};
