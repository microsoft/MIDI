// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2MidiSrvConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager,
    IMidiServicePluginMetadataReporterInterface>
{
public:
    // One issue in the new lib-based rpc implementation is this Initialize function is not in 
    // the interfaces used by the client, so midsrv is always nullptr. Therefore the individual
    // calls here do a lazy initialize if needed. I don't like that, but breaking the interface 
    // contracts now would be bad. This is a problem only for the Get__List reporting functions

    STDMETHOD(Initialize(
        _In_ GUID transportId, 
        _In_opt_ IMidiDeviceManagerInterface* deviceManagerInterface, 
        _In_opt_ IMidiServiceConfigurationManagerInterface* midiServiceConfigurationManagerInterface));

    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJson, _Out_ LPWSTR* responseJson));
    //STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJson, _Inout_ BSTR* responseJson));
    STDMETHOD(Shutdown)();

    STDMETHOD(GetTransportList(_Out_ LPWSTR* transportListJson));
    //STDMETHOD(GetTransportList(_In_ DWORD transportMetadataStructSize, _In_ PTRANSPORTMETADATA transportList, _Inout_ DWORD* transportCount);

    STDMETHOD(GetTransformList(_Out_ LPWSTR* transformListJson));

private:
    std::unique_ptr<CMidi2MidiSrv> m_MidiSrv;

    GUID m_TransportGuid;

    bool m_initialized{ false };
};


