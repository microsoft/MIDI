// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidi2LoopbackMidiBiDi :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiBiDi,
    IMidiCallback>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PABSTRACTIONCREATIONPARAMS, _In_ DWORD*, _In_opt_ IMidiCallback*, _In_ LONGLONG, _In_ GUID));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG, _In_ LONGLONG);
    STDMETHOD(Cleanup)();

private:
    //wil::com_ptr_nothrow<IMidiBiDi> m_linkedBiDi;

//    std::vector<wil::com_ptr_nothrow<CMidi2LoopbackMidiBiDi>> m_linkedBiDiConnections{};


    //wil::com_ptr_nothrow<IMidiCallback> m_linkedBiDiCallback;
    wil::com_ptr_nothrow <IMidiCallback> m_callback;

    LONGLONG m_callbackContext;

    std::wstring m_endpointId{};
};


