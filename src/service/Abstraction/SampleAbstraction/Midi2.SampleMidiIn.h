// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidi2SampleMidiIn : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiIn,
        IMidiCallback>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ DWORD *, _In_opt_ IMidiCallback *));
    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);
    STDMETHOD(Cleanup)();

private:
};


