// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


class CMidi2Ble1MidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager>

{
public:
    STDMETHOD(Initialize(_In_ GUID transportId, _In_ IMidiDeviceManager* midiDeviceManager, _In_ IMidiServiceConfigurationManager* midiServiceConfigurationManager));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJsonSection, _Out_ LPWSTR* Response));
    STDMETHOD(Shutdown)();

private:
    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;

};
