// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


class CMidi2KSAggregateMidiInProxy :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiCallback>
{
public:
    HRESULT Initialize(
        _In_ LPCWSTR endpointDeviceInterfaceId,
        _In_ HANDLE filter,
        _In_ UINT pinId,
        _In_ ULONG bufferSize,
        _In_ DWORD* mmcssTaskId,
        _In_ IMidiCallback* callback,
        _In_ LONGLONG context,
        _In_ BYTE group);

    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG, _In_ LONGLONG);

    HRESULT Shutdown();

private:
    std::wstring m_endpointDeviceId{};
    std::atomic<uint64_t> m_countMidiMessageSent{};

    wil::com_ptr_nothrow<IMidiDataTransform> m_bs2UmpTransform{ nullptr };

    IMidiCallback* m_callback{ nullptr };

    std::unique_ptr<KSMidiInDevice> m_device{ nullptr };

    LONGLONG m_context{};
    uint8_t m_groupIndex{};
};