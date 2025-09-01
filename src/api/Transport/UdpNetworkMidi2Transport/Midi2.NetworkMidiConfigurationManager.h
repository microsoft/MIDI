// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


class CMidi2NetworkMidiConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiTransportConfigurationManager>

{
public:
    STDMETHOD(Initialize(_In_ GUID transportId, _In_ IMidiDeviceManager* midiDeviceManager, _In_ IMidiServiceConfigurationManager* midiServiceConfigurationManager));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJsonSection, _Out_ LPWSTR* Response));
    STDMETHOD(Shutdown)();

    STDMETHOD(ValidateHostDefinition(_In_ MidiNetworkHostDefinition& definition, _Out_ winrt::hstring& errorMessage));
//    STDMETHOD(ValidateClientDefinition(_In_ MidiNetworkUdpClientDefinition& definition));

private:
    HRESULT ProcessCommand(
        _In_ json::JsonObject const& transportObject,
        _Inout_ json::JsonObject& responseObject) noexcept;

    HRESULT RunCommandEnumerateClients(_Inout_ json::JsonObject& responseObject) noexcept;
    HRESULT RunCommandEnumerateHosts(_Inout_ json::JsonObject& responseObject) noexcept;
    HRESULT RunCommandStopHost(
        _In_ winrt::hstring const& hostConfigEntryId,
        _Inout_ json::JsonObject& responseObject) noexcept;

    HRESULT RunCommandStartHost(
        _In_ winrt::hstring const& hostConfigEntryId,
        _Inout_ json::JsonObject& responseObject) noexcept;

    HRESULT RunCommandConnectDirect(
        _In_ winrt::hstring const& clientConfigEntryId,
        _In_ winrt::hstring const& remoteAddress,
        _In_ winrt::hstring const& remotePort,
        _In_ winrt::hstring const& umpEndpointName,
        _Inout_ json::JsonObject& responseObject) noexcept;


    HRESULT RunCommandDisconnectClient(
        _In_ winrt::hstring const& clientConfigEntryId,
        _In_ json::JsonObject& responseObject) noexcept;

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;

};
