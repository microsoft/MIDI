// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidiDevicePipe :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiCallback>
{
public:
    
    HRESULT Initialize(_In_ handle_t BindingHandle,
                            _In_ LPCWSTR,
                            _In_ PMIDISRV_CLIENTCREATION_PARAMS,
                            _In_ DWORD *);
    HRESULT Cleanup();

    HRESULT AddClientPipe(wil::com_ptr_nothrow<CMidiClientPipe>& MidiClientPipe)
    {
        auto lock = m_ClientPipeLock.lock();
        m_MidiClientPipes[(MidiClientHandle)MidiClientPipe.get()] = MidiClientPipe;
        return S_OK;
    }

    HRESULT RemoveClientPipe(wil::com_ptr_nothrow<CMidiClientPipe>& MidiClientPipe)
    {
        auto lock = m_ClientPipeLock.lock();

        auto item = m_MidiClientPipes.find((MidiClientHandle)MidiClientPipe.get());

        if (item != m_MidiClientPipes.end())
        {
            m_MidiClientPipes.erase(item);
            return S_OK;
        }

        return E_INVALIDARG;
    }

    HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    BOOL InUse()
    {
        auto lock = m_ClientPipeLock.lock();
        return !m_MidiClientPipes.empty();
    }

private:
    wil::critical_section m_ClientPipeLock;
    std::map<MidiClientHandle, wil::com_ptr_nothrow<CMidiClientPipe>> m_MidiClientPipes;

    wil::unique_hstring m_MidiDevice;
    MidiProtocol m_Protocol{ LegacyMidi };
    MidiFlow m_Flow{ MidiFlowIn };

    wil::critical_section m_DevicePipeLock;
    winrt::guid m_AbstractionGuid;
    wil::com_ptr_nothrow<IMidiBiDi> m_MidiBiDiDevice;
    wil::com_ptr_nothrow<IMidiIn> m_MidiInDevice;
    wil::com_ptr_nothrow<IMidiOut> m_MidiOutDevice;
};

