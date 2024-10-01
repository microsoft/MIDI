// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <libmidi2/bytestreamToUMP.h>

class CMidi2BS2UMPMidiTransform : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiDataTransform>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSFORMCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ IUnknown*));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Cleanup)();

private:

    std::wstring m_Device;
    wil::com_ptr_nothrow<IMidiCallback> m_Callback;
    bytestreamToUMP m_BS2UMP;

    uint32_t m_umpMessage[MAXIMUM_LOOPED_UMP_DATASIZE / sizeof(uint32_t)];
    uint8_t m_umpMessageCurrentWordCount{ 0 };
};


