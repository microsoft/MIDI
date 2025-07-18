// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <libmidi2/umpToBytestream.h>

class CMidi2UMP2BSMidiTransform : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiDataTransform>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSFORMCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ IMidiDeviceManager*));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Shutdown)();

private:

    std::wstring m_Device;
    wil::com_ptr_nothrow<IMidiCallback> m_Callback;

    umpToBytestream m_UMP2BS;
};


