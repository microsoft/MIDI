// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2VirtualMidiBidi :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiBidirectional,
    IMidiCallback>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSPORTCREATIONPARAMS, _In_ DWORD*, _In_opt_ IMidiCallback*, _In_ LONGLONG, _In_ GUID));
    STDMETHOD(SendMidiMessage(_In_ MessageOptionFlags, _In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Callback)(_In_ MessageOptionFlags, _In_ PVOID, _In_ UINT, _In_ LONGLONG, _In_ LONGLONG);
    STDMETHOD(Shutdown)();

    HRESULT LinkAssociatedCallback(_In_ wil::com_ptr_nothrow<IMidiCallback> callback)
    {
        if (m_linkedBidiCallback != nullptr)
        {
            m_linkedBidiCallback.reset();
        }

        m_linkedBidiCallback = callback;

        return S_OK;
    }

    HRESULT UnlinkAssociatedCallback()
    {
        if (m_linkedBidi != nullptr)
        {
            m_linkedBidi.reset();
        }

        if (m_linkedBidiCallback != nullptr)
        {
            m_linkedBidiCallback.reset();
        }

        return S_OK;
    }

    //IMidiCallback* GetCallback()
    //{
    //    return m_callback.get();
    //}

private:
    wil::com_ptr_nothrow<IMidiBidirectional> m_linkedBidi;

    //std::vector<wil::com_ptr_nothrow<CMidi2VirtualMidiBidi>> m_linkedBidiConnections{};

    wil::com_ptr_nothrow<IMidiCallback> m_linkedBidiCallback;
    wil::com_ptr_nothrow<IMidiCallback> m_callback;
    LONGLONG m_callbackContext;

    std::wstring m_endpointId{};

    GUID m_sessionId{};

    bool m_isDeviceSide{ false };

};


