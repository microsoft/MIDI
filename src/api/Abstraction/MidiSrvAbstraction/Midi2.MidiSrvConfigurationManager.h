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
    STDMETHOD(Initialize(
        _In_ GUID transportId, 
        _In_ IMidiDeviceManagerInterface* deviceManagerInterface, 
        _In_ IMidiServiceConfigurationManagerInterface* midiServiceConfigurationManagerInterface));

    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJson, _Out_ LPWSTR* responseJson));
    STDMETHOD(Shutdown)();


    STDMETHOD(GetTransportList(_Out_ LPWSTR* transportListJson));
    STDMETHOD(GetTransformList(_Out_ LPWSTR* transformListJson));

private:
    std::unique_ptr<CMidi2MidiSrv> m_MidiSrv;

    GUID m_abstractionGuid;
};


