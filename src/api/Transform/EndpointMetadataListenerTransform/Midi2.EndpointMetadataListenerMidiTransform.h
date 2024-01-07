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
    HRESULT UpdateFunctionBlockNameProperty(_In_ uint8_t functionBlockNumber, _In_ std::wstring value);

    HRESULT UpdateDeviceIdentityProperty(_In_ internal::PackedUmp128& identityMessage);
    HRESULT UpdateEndpointInfoProperties(_In_ internal::PackedUmp128& endpointInfoNotificationMessage);
    HRESULT UpdateStreamConfigurationProperties(_In_ internal::PackedUmp128& endpointStreamConfigurationNotificationMessage);
    HRESULT UpdateFunctionBlockProperty(_In_ internal::PackedUmp128& functionBlockInfoNotificationMessage);

    HRESULT HandleFunctionBlockNameMessage(_In_ internal::PackedUmp128& functionBlockNameMessage);
    HRESULT HandleEndpointNameMessage(_In_ internal::PackedUmp128& endpointNameMessage);
    HRESULT HandleProductInstanceIdMessage(_In_ internal::PackedUmp128& productInstanceIdMessage);

    HRESULT ProcessStreamMessage(_In_ internal::PackedUmp128 ump, _In_ LONGLONG timestamp);


    IMidiCallback* m_callback{ nullptr };
    LONGLONG m_context{ 0 };

    std::wstring m_deviceInstanceId;

    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;

    // these are holding locations while names are built
    std::wstring m_endpointName{};
    std::wstring m_productInstanceId{};
    std::map<uint8_t /* function block number */, std::wstring> m_functionBlockNames;
};

