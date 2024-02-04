// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidi2VirtualMidiBiDi :
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

    HRESULT LinkAssociatedBiDi(_In_ wil::com_ptr_nothrow<CMidi2VirtualMidiBiDi> biDi)
    {
        m_linkedBiDi = biDi;

        RETURN_IF_FAILED(biDi->QueryInterface(__uuidof(IMidiCallback), (void**)&m_linkedBiDiCallback));

        return S_OK;
    }

    HRESULT UnlinkAssociatedBiDi()
    {
        m_linkedBiDi = nullptr;
        m_linkedBiDiCallback = nullptr;

        return S_OK;
    }

private:
    wil::com_ptr_nothrow<IMidiBiDi> m_linkedBiDi;

    wil::com_ptr_nothrow<IMidiCallback> m_linkedBiDiCallback;
    wil::com_ptr_nothrow <IMidiCallback> m_callback;

    LONGLONG m_callbackContext;

    std::wstring m_endpointId{};


    bool m_isDeviceSide{ false };

};


