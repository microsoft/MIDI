// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2MidiSynthConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiTransportConfigurationManager>
{
public:
    STDMETHOD(Initialize(_In_ GUID transportId, _In_ IMidiDeviceManager* midiDeviceManager, _In_ IMidiServiceConfigurationManager* midiServiceConfigurationManager));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJsonSection, _Out_ LPWSTR* response));
    STDMETHOD(Shutdown)();

private:
    HRESULT ProcessCommand(
        _In_ json::JsonObject const& transportObject,
        _Inout_ json::JsonObject& responseObject);

    HRESULT ProcessSettings(
        _In_ json::JsonObject const& jsonObject,
        _Inout_ json::JsonObject& responseObject);

    void AddCurrentSettingsToResponse(_Inout_ json::JsonObject& responseObject);

    // Nothing reads this yet. It is held, and let go in Shutdown, like every other transport's.
    wil::srwlock m_serviceLock;
    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
};
