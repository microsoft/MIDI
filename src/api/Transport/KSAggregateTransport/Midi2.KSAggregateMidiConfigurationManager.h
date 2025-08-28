// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointMatchCriteria.h"
#include "MidiEndpointCustomPropertiesCache.h"
#include "MidiEndpointNameTable.h"

class CMidi2KSAggregateMidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager>
{
public:

    STDMETHOD(Initialize(
        _In_ GUID transportId, 
        _In_ IMidiDeviceManager* deviceManagerInterface, 
        _In_ IMidiServiceConfigurationManager* midiServiceConfigurationManagerInterface));
    
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJson, _Out_ LPWSTR* responseJson));
    STDMETHOD(Shutdown)();

    // internal method called after endpoint creation
  //  HRESULT ApplyConfigFileUpdatesForEndpoint(_In_ std::wstring endpointSearchKeysJson);

    std::shared_ptr<MidiEndpointCustomPropertiesCache> CustomPropertiesCache() { return m_customPropertiesCache; }


private:
    HRESULT ProcessCommand(
        _In_ json::JsonObject const& transportObject,
        _In_ json::JsonObject& responseObject);

    HRESULT ProcessCustomProperties(
        _In_ winrt::hstring resolvedEndpointDeviceId,
        _In_ std::shared_ptr<MidiEndpointMatchCriteria> matchCriteria,
        _In_ json::JsonObject updateObject,
        _In_ std::shared_ptr<MidiEndpointCustomProperties>& customProperties,
        _In_ std::vector<DEVPROPERTY>& endpointDevProperties,
        _In_ std::shared_ptr<MidiEndpointNameTable>& nameTable,
        _In_ json::JsonObject& responseObject);


    std::shared_ptr<MidiEndpointCustomPropertiesCache> m_customPropertiesCache{ std::make_shared<MidiEndpointCustomPropertiesCache>() };

    wil::com_ptr_nothrow<IMidiServiceConfigurationManager> m_midiServiceConfigurationManagerInterface;
    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;

    //HRESULT ApplyUserConfiguration(_In_ std::wstring deviceInterfaceId);

};