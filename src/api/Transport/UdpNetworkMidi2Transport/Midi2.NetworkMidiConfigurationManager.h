// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


class CMidi2NetworkMidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager>

{
public:
    STDMETHOD(Initialize(_In_ GUID transportId, _In_ IMidiDeviceManagerInterface* midiDeviceManager, _In_ IMidiServiceConfigurationManagerInterface* midiServiceConfigurationManagerInterface));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJsonSection, _Out_ LPWSTR* Response));
    STDMETHOD(Shutdown)();

    STDMETHOD(ValidateHostDefinition(_In_ MidiNetworkHostDefinition& definition, _Out_ winrt::hstring& errorMessage));
//    STDMETHOD(ValidateClientDefinition(_In_ MidiNetworkUdpClientDefinition& definition));

private:
    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_midiDeviceManager;

    GUID m_transportId;   // kept for convenience
};
