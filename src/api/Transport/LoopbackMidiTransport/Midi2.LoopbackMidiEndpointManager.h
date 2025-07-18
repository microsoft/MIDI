// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


class CMidi2LoopbackMidiEndpointManager :
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

    HRESULT CreateEndpointPair(
        _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionA,
        _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionB
    );

    HRESULT DeleteEndpointPair(
        _In_ MidiLoopbackDeviceDefinition const& definitionA,
        _In_ MidiLoopbackDeviceDefinition const& definitionB
    );

    bool IsInitialized() { return m_initialized; }


    //HRESULT DeleteEndpointPair(
    //    _In_ GUID const VirtualEndpointAssociationGuid
    //);

private:
    HRESULT ProcessWorkQueue();

    bool m_initialized{ false };

    void CleanupDeviceDefinition(_In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definition)
    {
        definition->AssociationId = internal::ToLowerTrimmedWStringCopy(definition->AssociationId);
        internal::InPlaceTrim(definition->EndpointUniqueIdentifier);
        internal::InPlaceTrim(definition->InstanceIdPrefix);
        internal::InPlaceTrim(definition->EndpointName);
        internal::InPlaceTrim(definition->EndpointDescription);
    }

    HRESULT CreateSingleEndpoint(_In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definition);
    HRESULT DeleteSingleEndpoint(_In_ MidiLoopbackDeviceDefinition const& definition);


    GUID m_ContainerId{};
    GUID m_TransportTransportId{};

    std::wstring m_parentDeviceId{};

    HRESULT CreateParentDevice();

    wil::com_ptr_nothrow<IMidiDeviceManager> m_MidiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_MidiProtocolManager;

};
