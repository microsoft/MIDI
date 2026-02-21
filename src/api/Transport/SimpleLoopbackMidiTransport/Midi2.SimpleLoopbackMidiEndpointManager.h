// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


class CMidi2SimpleLoopbackMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    //STDMETHOD(ApplyConfiguration(
    //    _In_ LPCWSTR configurationJson,
    //    _Out_ LPWSTR resultJson
    //));

    HRESULT CreateEndpoint(
        _In_ std::shared_ptr<MidiSimpleLoopbackDeviceDefinition> definition
    );

    HRESULT DeleteEndpoint(
        _In_ MidiSimpleLoopbackDeviceDefinition const& definition
    );

    bool IsInitialized() { return m_initialized; }

private:
    HRESULT ProcessWorkQueue();

    bool m_initialized{ false };

    void CleanupDeviceDefinition(_In_ std::shared_ptr<MidiSimpleLoopbackDeviceDefinition> definition)
    {
        definition->AssociationId = internal::ToLowerTrimmedWStringCopy(definition->AssociationId);
        internal::InPlaceTrim(definition->EndpointUniqueIdentifier);
        internal::InPlaceTrim(definition->InstanceIdPrefix);
        internal::InPlaceTrim(definition->EndpointName);
        internal::InPlaceTrim(definition->EndpointDescription);
    }


    GUID m_ContainerId{};
    GUID m_TransportTransportId{};

    std::wstring m_parentDeviceId{};

    HRESULT CreateParentDevice();

    wil::com_ptr_nothrow<IMidiDeviceManager> m_MidiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_MidiProtocolManager;

};
