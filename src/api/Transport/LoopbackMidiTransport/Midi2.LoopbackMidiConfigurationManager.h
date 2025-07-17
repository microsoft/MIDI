// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


class CMidi2LoopbackMidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager>

{
public:
    STDMETHOD(Initialize(_In_ GUID transportId, _In_ IMidiDeviceManager* MidiDeviceManager, _In_ IMidiServiceConfigurationManager* MidiServiceConfigurationManagerInterface));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR ConfigurationJsonSection, _Out_ LPWSTR* Response));
    STDMETHOD(Shutdown)();

private:
    wil::com_ptr_nothrow<IMidiDeviceManager> m_MidiDeviceManager;

    GUID m_TransportId;   // kept for convenience
};
