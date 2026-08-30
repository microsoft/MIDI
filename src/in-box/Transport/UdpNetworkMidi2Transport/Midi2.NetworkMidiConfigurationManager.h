// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointMatchCriteria.h"
#include "MidiEndpointCustomPropertiesCache.h"


class CMidi2NetworkMidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager>

{
public:
    STDMETHOD(Initialize(_In_ GUID transportId, _In_ IMidiDeviceManager* midiDeviceManager, _In_ IMidiServiceConfigurationManager* midiServiceConfigurationManager));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJsonSection, _Out_ LPWSTR* Response));
    STDMETHOD(Shutdown)();

    STDMETHOD(ValidateHostDefinition(_In_ MidiNetworkHostDefinition& definition, _Out_ winrt::hstring& errorMessage, _Out_ uint32_t& errorCode));
//    STDMETHOD(ValidateClientDefinition(_In_ MidiNetworkUdpClientDefinition& definition));

    // Custom names and descriptions the user has chosen, keyed by match criteria. A network
    // endpoint is built long after the configuration arrives, so this is consulted at creation
    // time and the name is set before the device node is activated. That is what keeps a
    // user-named connection from being created under its default name and renamed a moment
    // later, which would churn the endpoint and its MIDI 1.0 ports.
    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomPropertiesCache> CustomPropertiesCache() { return m_customPropertiesCache; }

private:
    HRESULT ProcessEndpointCustomizations(
        _In_ json::JsonObject const& jsonObject,
        _Inout_ json::JsonObject& responseObject) noexcept;

    std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomPropertiesCache> m_customPropertiesCache{ std::make_shared<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomPropertiesCache>() };

    HRESULT ProcessCommand(
        _In_ json::JsonObject const& transportObject,
        _Inout_ json::JsonObject& responseObject) noexcept;

    
    HRESULT RunCommandGetPendingRemoteClients(_Inout_ json::JsonObject& responseObject) noexcept;
    HRESULT RunCommandEnumerateClients(_Inout_ json::JsonObject& responseObject) noexcept;
    HRESULT RunCommandEnumerateHosts(_Inout_ json::JsonObject& responseObject) noexcept;
    HRESULT RunCommandGetTransportSettings(_Inout_ json::JsonObject& responseObject) noexcept;
    HRESULT RunCommandStopHost(
        _In_ winrt::guid const& hostConfigEntryId,
        _Inout_ json::JsonObject& responseObject) noexcept;

    HRESULT RunCommandRemoveHost(
        _In_ winrt::guid const& hostConfigEntryId,
        _Inout_ json::JsonObject& responseObject) noexcept;

    HRESULT RunCommandStartHost(
        _In_ winrt::guid const& hostConfigEntryId,
        _Inout_ json::JsonObject& responseObject) noexcept;

    HRESULT RunCommandConnectDirect(
        _In_ winrt::guid const& clientConfigEntryId,
        _In_ winrt::hstring const& remoteAddress,
        _In_ winrt::hstring const& remotePort,
        _In_ winrt::hstring const& umpEndpointName,
        _In_ winrt::hstring const& customEndpointName,
        _In_ bool const createMidi1Ports,
        _Inout_ json::JsonObject& responseObject) noexcept;

    // Connects to an mDNS-discovered host by its Windows device id. The address and port are
    // resolved from the advertisement each time the host is seen, so this survives the remote
    // moving to a new address, which a direct connection cannot.
    HRESULT RunCommandConnectMdns(
        _In_ winrt::guid const& clientConfigEntryId,
        _In_ winrt::hstring const& matchId,
        _In_ winrt::hstring const& umpEndpointName,
        _In_ winrt::hstring const& customEndpointName,
        _In_ bool const createMidi1Ports,
        _Inout_ json::JsonObject& responseObject) noexcept;

    HRESULT RunCommandDisconnectClient(
        _In_ winrt::guid const& clientConfigEntryId,
        _Inout_ json::JsonObject& responseObject) noexcept;

    // Ends one remote client's session with a host on this PC. Records no decision, so the
    // remote may invite itself back in; denyRemoteClient is what refuses it for good.
    HRESULT RunCommandDisconnectRemoteClient(
        _In_ winrt::guid const& hostEntryId,
        _In_ MidiNetworkRemoteClientIdentity const& identity,
        _Inout_ json::JsonObject& responseObject) noexcept;

    // A user's approve or deny decision for a remote client on one of our hosts. persist means
    // the caller chose "always", which is also written to the configuration file by the caller.
    HRESULT RunCommandRemoteClientDecision(
        _In_ winrt::guid const& hostEntryId,
        _In_ MidiNetworkRemoteClientIdentity const& identity,
        _In_ bool const approve,
        _In_ bool const persist,
        _Inout_ json::JsonObject& responseObject) noexcept;

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;

};
