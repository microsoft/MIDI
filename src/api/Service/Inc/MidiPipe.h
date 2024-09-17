// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

typedef ULONGLONG MidiPipeHandle;

class CMidiPipe :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiCallback>
{
public:
    virtual ~CMidiPipe()
    {
        Cleanup();
    }

    virtual HRESULT Initialize(_In_ LPCWSTR Device,
        _In_ MidiFlow Flow)
    {
        m_Device = internal::NormalizeEndpointInterfaceIdWStringCopy(Device);
        m_Flow = Flow;

        if (IsFlowSupported(MidiFlowIn))
        {
            m_DataFormatIn = MidiDataFormat_Any;
        }

        if (IsFlowSupported(MidiFlowOut))
        {
            m_DataFormatOut = MidiDataFormat_Any;
        }

        return S_OK;
    }

    virtual HRESULT Cleanup()
    {
        auto lock = m_Lock.lock();
        m_ConnectedPipes.clear();
        m_Clients.clear();

        return S_OK;
    };

    virtual HRESULT AddConnectedPipe(wil::com_ptr_nothrow<CMidiPipe>& ConnectedOutputPipe)
    {
        auto lock = m_Lock.lock();
        m_ConnectedPipes[(MidiPipeHandle)ConnectedOutputPipe.get()] = ConnectedOutputPipe;

        return S_OK;
    }
    
    virtual HRESULT RemoveConnectedPipe(wil::com_ptr_nothrow<CMidiPipe>& ConnectedOutputPipe)  
    {
        auto lock = m_Lock.lock();

        auto item = m_ConnectedPipes.find((MidiPipeHandle)ConnectedOutputPipe.get());

        if (item != m_ConnectedPipes.end())
        {
            m_ConnectedPipes.erase(item);
            return S_OK;
        }

        return E_INVALIDARG;
    }

    virtual HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG) { return E_NOTIMPL; }
    virtual HRESULT SendMidiMessageNow(_In_ PVOID, _In_ UINT, _In_ LONGLONG) { return E_NOTIMPL; }

    STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Length, _In_ LONGLONG Position, _In_ LONGLONG)
    {
        // need to hold the client pipe lock to ensure that
        // no clients are added or removed while performing the callback
        // to the client.
        auto lock = m_Lock.lock();

        for (auto const& Client : m_ConnectedPipes)
        {
            LOG_IF_FAILED(Client.second->SendMidiMessage(Data, Length, Position));
        }

        return S_OK;
    }

    std::wstring MidiDevice() { return m_Device; }
    MidiDataFormat DataFormatIn() { return m_DataFormatIn; }
    MidiDataFormat DataFormatOut() { return m_DataFormatOut; }
    MidiFlow Flow() { return m_Flow; }
    BOOL IsFlowSupported(MidiFlow Flow) { return (m_Flow == MidiFlowBidirectional || m_Flow == Flow); }

    virtual HRESULT SetDataFormatIn(MidiDataFormat DataFormat)
    {
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), !IsFormatSupportedIn(DataFormat));
        m_DataFormatIn = DataFormat;
        return S_OK;
    }

    virtual HRESULT SetDataFormatOut(MidiDataFormat DataFormat)
    {
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), !IsFormatSupportedOut(DataFormat));
        m_DataFormatOut = DataFormat;
        return S_OK;
    }

    virtual BOOL IsFormatSupportedIn(MidiDataFormat DataFormat)
    {
        if (m_DataFormatIn == MidiDataFormat_Any)
        {
            return TRUE;
        }
        else
        {
            return m_DataFormatIn == DataFormat;
        }
    }

    virtual BOOL IsFormatSupportedOut(MidiDataFormat DataFormat)
    {
        if (m_DataFormatOut == MidiDataFormat_Any)
        {
            return TRUE;
        }
        else
        {
           return m_DataFormatOut == DataFormat;
        }
    }

    virtual BOOL InUse()
    {
        auto lock = m_Lock.lock();
        return !m_Clients.empty();
    }

    virtual void AddClient(MidiClientHandle Handle)
    {
        auto lock = m_Lock.lock();
        auto client = std::find(m_Clients.begin(), m_Clients.end(), Handle);
        if (client == m_Clients.end())
        {
            m_Clients.push_back(Handle);
        }
    }

    virtual void RemoveClient(MidiClientHandle Handle)
    {
        auto lock = m_Lock.lock();
        auto client = std::find(m_Clients.begin(), m_Clients.end(), Handle);
        if (client != m_Clients.end())
        {
            m_Clients.erase(client);
        }
        else
        {
            LOG_IF_FAILED(E_NOTFOUND);
        }
    }

private:
    std::wstring m_Device;
    MidiDataFormat m_DataFormatIn{ MidiDataFormat_Invalid };
    MidiDataFormat m_DataFormatOut{ MidiDataFormat_Invalid };
    MidiFlow m_Flow{ MidiFlowIn };

    wil::critical_section m_Lock;
    std::map<MidiPipeHandle, wil::com_ptr_nothrow<CMidiPipe>> m_ConnectedPipes;
    std::vector<MidiClientHandle> m_Clients;

};

