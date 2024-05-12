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
        _In_ LPCWSTR Device,
        _In_ HANDLE Filter,
        _In_ UINT PinId,
        _In_ ULONG BufferSize,
        _In_ DWORD* MmcssTaskId,
        _In_ IMidiCallback* Callback,
        _In_ LONGLONG Context,
        _In_ BYTE Group);

    HRESULT Callback(_In_ PVOID, _In_ UINT, _In_ LONGLONG, _In_ LONGLONG);

    HRESULT Cleanup();

private:
    bytestreamToUMP m_BS2UMP;
        
    IMidiCallback* m_callback{ nullptr };

    std::unique_ptr<KSMidiInDevice> m_device{ nullptr };

    LONGLONG m_context{};
    uint8_t m_groupIndex{};
};