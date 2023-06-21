// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidiDevicePipe :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiCallback>
{
public:
    
    HRESULT Initialize(_In_ handle_t BindingHandle,
                            _In_ HANDLE,
                            _In_ LPCWSTR,
                            _In_ PMIDISRV_CLIENTCREATION_PARAMS,
                            _In_ DWORD *);
    HRESULT Cleanup();

    HRESULT AddClientPipe(wil::com_ptr_nothrow<CMidiClientPipe>& MidiClientPipe)
    {
        m_MidiClientPipe = MidiClientPipe;

        return S_OK;
    }

    HRESULT RemoveClientPipe(wil::com_ptr_nothrow<CMidiClientPipe>& MidiClientPipe)
    {
        if (m_MidiClientPipe == MidiClientPipe)
        {
            m_MidiClientPipe.reset();
            return S_OK;
        }
        return E_INVALIDARG;
    }

    HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

private:
    wil::com_ptr_nothrow<CMidiClientPipe> m_MidiClientPipe;

    wil::unique_hstring m_MidiDevice;
    MidiProtocol m_Protocol{ LegacyMidi };
    MidiFlow m_Flow{ MidiFlowIn };

    wil::com_ptr_nothrow<IMidiBiDi> m_KSMidiBiDiDevice;
    wil::com_ptr_nothrow<IMidiIn> m_KSMidiInDevice;
    wil::com_ptr_nothrow<IMidiOut> m_KSMidiOutDevice;
};

