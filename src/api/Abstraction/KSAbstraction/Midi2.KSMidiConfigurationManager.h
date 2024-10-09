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
    IMidiAbstractionConfigurationManager>
{
public:

    STDMETHOD(Initialize(_In_ GUID abstractionGuid, _In_ IMidiDeviceManagerInterface* deviceManagerInterface, _In_ IMidiServiceConfigurationManagerInterface* MidiServiceConfigurationManagerInterface));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJson, _In_ BOOL isFromConfigurationFile, _Out_ BSTR* response));
    STDMETHOD(Shutdown)();

    // internal method called after endpoint creation
    HRESULT ApplyConfigFileUpdatesForEndpoint(_In_ std::wstring endpointSearchKeysJson);


    // internal helper methods specific to this abstraction

    std::wstring BuildEndpointJsonSearchKeysForSWD(_In_ std::wstring endpointDeviceInterfaceId);
    // TODO: Other helper methods for other types of criterial


private:
//    std::vector<std::unique_ptr<ConfigUpdateForEndpoint>> m_CachedConfigurationUpdates{};

    wil::com_ptr_nothrow<IMidiServiceConfigurationManagerInterface> m_MidiServiceConfigurationManagerInterface;
    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;

    //HRESULT ApplyUserConfiguration(_In_ std::wstring deviceInterfaceId);

};