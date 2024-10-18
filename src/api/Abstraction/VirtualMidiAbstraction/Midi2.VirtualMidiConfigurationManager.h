// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


class CMidi2VirtualMidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager>

{
public:
    STDMETHOD(Initialize(_In_ GUID AbstractionId, _In_ IMidiDeviceManagerInterface* MidiDeviceManager, _In_ IMidiServiceConfigurationManagerInterface* MidiServiceConfigurationManagerInterface));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJsonSection, _Out_ LPWSTR* response));
    STDMETHOD(Shutdown)();

private:
    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;

    GUID m_abstractionId;   // kept for convenience
};
