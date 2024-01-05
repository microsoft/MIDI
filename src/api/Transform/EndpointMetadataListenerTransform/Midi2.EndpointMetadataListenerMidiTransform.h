// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


class CMidi2EndpointMetadataListenerMidiTransform :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiDataTransform>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSFORMCREATIONPARAMS, _In_ DWORD*, _In_opt_ IMidiCallback*, _In_ LONGLONG, _In_ IUnknown*));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Cleanup)();

private:
    HRESULT UpdateEndpointNameProperty();
    HRESULT UpdateEndpointProductInstanceIdProperty();
    HRESULT UpdateDeviceIdentityProperty(_In_ internal::PackedUmp128& identity);

    // this pulls the existing property, updates it with new data, and then 
    // writes it back to the store. It's more complex than the other property
    // updates.
    HRESULT UpdateFunctionBlocksProperty();

    HRESULT ProcessStreamMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG timestamp);


    // this is responsible for adding to or updating the function block list, or to the 
    // function block name list. For the name list, it handles restarting etc.
    HRESULT AddOrUpdateInternalFunctionBlockList(_In_ internal::PackedUmp128& relatedMessage);


    IMidiCallback* m_callback{ nullptr };
    LONGLONG m_context{ 0 };

    std::wstring m_deviceInstanceId;

    std::vector<internal::PackedUmp128> m_queuedEndpointNameMessages;
    std::vector<internal::PackedUmp128> m_queuedEndpointProductInstanceIdMessages;

    // list of function blocks ready to be written to the properties.
    // The Size property in these won't be valid until we get the name for any given block
    std::vector<MidiFunctionBlockHeader> m_functionBlocks;

    // list of messages for the function block names. The uint8_t is the function block number
    std::map<uint8_t, std::vector<internal::PackedUmp128>> m_functionBlockNameMessages;

};

