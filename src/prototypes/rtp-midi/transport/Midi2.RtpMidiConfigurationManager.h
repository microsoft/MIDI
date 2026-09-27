// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Reads the transport's configuration section and runs its commands.
// ============================================================================

#pragma once

class CMidi2RtpMidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiTransportConfigurationManager>
{
public:
    STDMETHOD(Initialize)(_In_ GUID transportId, _In_ IMidiDeviceManager* midiDeviceManager, _In_ IMidiServiceConfigurationManager* midiServiceConfigurationManager);
    STDMETHOD(UpdateConfiguration)(_In_ LPCWSTR configurationJsonSection, _Out_ LPWSTR* response);
    STDMETHOD(Shutdown)();

    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomPropertiesCache> CustomPropertiesCache() { return m_customPropertiesCache; }

private:
    void ProcessCreateSection(_In_ json::JsonObject const& createSection, _Inout_ json::JsonObject& responseObject);
    void ProcessEndpointCustomizations(_In_ json::JsonObject const& section);
    void ProcessEndpointCustomizationRemovals(_In_ json::JsonObject const& removeSection);
    void ProcessCommand(_In_ json::JsonObject const& section, _Inout_ json::JsonObject& responseObject);

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;

    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomPropertiesCache> m_customPropertiesCache{
        std::make_shared<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomPropertiesCache>() };
};
