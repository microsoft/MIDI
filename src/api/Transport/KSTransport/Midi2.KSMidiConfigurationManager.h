// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


class CMidi2KSMidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager>
{
public:

    STDMETHOD(Initialize(_In_ GUID transportId, _In_ IMidiDeviceManager* deviceManagerInterface, _In_ IMidiServiceConfigurationManager* MidiServiceConfigurationManagerInterface));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJson, _Out_ LPWSTR* responseJson));
    STDMETHOD(Shutdown)();

    // internal method called after endpoint creation
    HRESULT ApplyConfigFileUpdatesForEndpoint(_In_ std::wstring endpointSearchKeysJson);


    // internal helper methods specific to this transport

    std::wstring BuildEndpointJsonSearchKeysForSWD(_In_ std::wstring endpointDeviceInterfaceId);
    // TODO: Other helper methods for other types of criterial


private:
//    std::vector<std::unique_ptr<ConfigUpdateForEndpoint>> m_CachedConfigurationUpdates{};

    wil::com_ptr_nothrow<IMidiServiceConfigurationManager> m_midiServiceConfigurationManagerInterface;
    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;

    //HRESULT ApplyUserConfiguration(_In_ std::wstring deviceInterfaceId);

};