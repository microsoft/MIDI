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
    IMidiAbstractionConfigurationManager,
    IMidiServicePluginMetadataReporterInterface>
{
public:
    STDMETHOD(Initialize(_In_ GUID abstractionGuid, _In_ IMidiDeviceManagerInterface* deviceManagerInterface, _In_ IMidiServiceConfigurationManagerInterface* midiServiceConfigurationManagerInterface));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJson, _In_ BOOL IsFromConfigurationFile, _Out_ BSTR* response));
    STDMETHOD(Shutdown)();


    STDMETHOD(GetAbstractionList(_Out_ BSTR* AbstractionListJson));
    STDMETHOD(GetTransformList(_Out_ BSTR* TransformListJson));

private:
    std::unique_ptr<CMidi2MidiSrv> m_MidiSrv;

    GUID m_abstractionGuid;
};


