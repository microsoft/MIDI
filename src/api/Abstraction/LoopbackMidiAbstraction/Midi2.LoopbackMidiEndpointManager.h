// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


class CMidi2LoopbackMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IUnknown*, _In_ IUnknown*));
    STDMETHOD(Cleanup)();

    //STDMETHOD(ApplyConfiguration(
    //    _In_ LPCWSTR configurationJson,
    //    _Out_ LPWSTR resultJson
    //));

    HRESULT CreateEndpointPair(
        _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionA,
        _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionB,
        _In_ bool isFromConfigurationFile
    );

    HRESULT DeleteEndpointPair(
        _In_ MidiLoopbackDeviceDefinition const& definitionA,
        _In_ MidiLoopbackDeviceDefinition const& definitionB
    );




    //HRESULT DeleteEndpointPair(
    //    _In_ GUID const VirtualEndpointAssociationGuid
    //);

private:

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
    GUID m_TransportAbstractionId{};

    std::wstring m_parentDeviceId{};

    HRESULT CreateParentDevice();

    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManagerInterface> m_MidiProtocolManager;

};
