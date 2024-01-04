// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include "bytestreamToUMP.h"

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
    LONGLONG m_Context;
    bytestreamToUMP m_BS2UMP;
};


