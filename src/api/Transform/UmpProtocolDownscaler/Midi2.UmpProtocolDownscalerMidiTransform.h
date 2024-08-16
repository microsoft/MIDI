// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

using namespace winrt::Windows::Devices::Enumeration;
//
// Despite the name, this does both downscaling and upscaling
// New name should really be CMidi2UmpProtocolMidiTransform
//
class CMidi2UmpProtocolDownscalerMidiTransform :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiDataTransform>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSFORMCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ IUnknown*));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Cleanup)();

private:
    umpToMIDI1Protocol m_umpToMidi1{};
  //  umpToMIDI2Protocol m_umpToMidi2{};

    // this gets set based on the protocol negotiated, and also if we're using
    // the UMP driver but talking to a bytestream endpoint
    bool m_downscalingRequiredForEndpoint{ true };
    bool m_upscalingRequiredForEndpoint{ false };

    bool m_endpointSupportsMidi1Protocol{ false };
    bool m_endpointSupportsMidi2Protocol{ false };

    std::wstring m_Device;
    wil::com_ptr_nothrow<IMidiCallback> m_Callback;
    LONGLONG m_Context;

    HRESULT OnDeviceAdded(_In_ DeviceWatcher, _In_ DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ DeviceWatcher, _In_ DeviceInformationUpdate);

    DeviceWatcher m_Watcher{ nullptr };
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<IDeviceWatcher>::Updated_revoker m_DeviceUpdated;



};


