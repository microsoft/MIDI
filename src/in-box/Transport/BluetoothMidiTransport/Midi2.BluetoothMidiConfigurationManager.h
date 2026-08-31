// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "MidiEndpointCustomPropertiesCache.h"

class CMidi2BluetoothMidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager>

{
public:
    STDMETHOD(Initialize(_In_ GUID transportId, _In_ IMidiDeviceManager* midiDeviceManager, _In_ IMidiServiceConfigurationManager* midiServiceConfigurationManager));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJsonSection, _Out_ LPWSTR* Response));
    STDMETHOD(Shutdown)();

    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomPropertiesCache> CustomPropertiesCache() { return m_customPropertiesCache; }

private:
    HRESULT ProcessEndpointCustomizations(
        _In_ json::JsonObject const& jsonObject,
        _Inout_ json::JsonObject& responseObject) noexcept;

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;

    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomPropertiesCache> m_customPropertiesCache{ std::make_shared<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomPropertiesCache>() };
};
