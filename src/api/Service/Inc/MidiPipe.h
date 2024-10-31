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
        Shutdown();
    }

    virtual HRESULT Initialize(_In_ LPCWSTR device,
        _In_ MidiFlow flow)
    {
        m_Device = internal::NormalizeEndpointInterfaceIdWStringCopy(device);
        m_Flow = flow;

        if (IsFlowSupported(MidiFlowIn))
        {
            m_DataFormatIn = MidiDataFormats_Any;
        }

        if (IsFlowSupported(MidiFlowOut))
        {
            m_DataFormatOut = MidiDataFormats_Any;
        }

        return S_OK;
    }

    virtual HRESULT Shutdown()
    {
        auto lock = m_Lock.lock();
        m_ConnectedPipes.clear();
        m_Clients.clear();

        return S_OK;
    };

    virtual HRESULT AddConnectedPipe(wil::com_ptr_nothrow<CMidiPipe>& connectedOutputPipe)
    {
        auto lock = m_Lock.lock();
        m_ConnectedPipes[(MidiPipeHandle)connectedOutputPipe.get()] = connectedOutputPipe;

        return S_OK;
    }
    
    virtual HRESULT RemoveConnectedPipe(wil::com_ptr_nothrow<CMidiPipe>& connectedOutputPipe)  
    {
        auto lock = m_Lock.lock();

        auto item = m_ConnectedPipes.find((MidiPipeHandle)connectedOutputPipe.get());

        if (item != m_ConnectedPipes.end())
        {
            m_ConnectedPipes.erase(item);
            return S_OK;
        }

        return E_INVALIDARG;
    }

    virtual HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG) { return E_NOTIMPL; }
    virtual HRESULT SendMidiMessageNow(_In_ PVOID, _In_ UINT, _In_ LONGLONG) { return E_NOTIMPL; }

    STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT length, _In_ LONGLONG position, _In_ LONGLONG context)
    {
        // need to hold the client pipe lock to ensure that
        // no clients are added or removed while performing the callback
        // to the client.
        auto lock = m_Lock.lock();

        for (auto const& Client : m_ConnectedPipes)
        {
            // By convention, context will contain the group index this message is targeting.
            // If this client is group filtered, and the context is a valid group index, then
            // filter the messages being sent to this client by the group index.
            // otherwise, send all messages to the client.
            if (Client.second->IsGroupFiltered() && IS_VALID_GROUP_INDEX(context))
            {
                if (Client.second->GroupIndex() == (UINT32) context)
                {
                    LOG_IF_FAILED(Client.second->SendMidiMessage(Data, length, position));
                }
            }
            else
            {
                LOG_IF_FAILED(Client.second->SendMidiMessage(Data, length, position));
            }
        }

        return S_OK;
    }

    std::wstring MidiDevice() { return m_Device; }
    MidiDataFormats DataFormatIn() { return m_DataFormatIn; }
    MidiDataFormats DataFormatOut() { return m_DataFormatOut; }
    MidiFlow Flow() { return m_Flow; }
    BOOL IsFlowSupported(MidiFlow flow) { return (m_Flow == MidiFlowBidirectional || m_Flow == flow); }

    virtual HRESULT SetDataFormatIn(MidiDataFormats dataFormat)
    {
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), !IsFormatSupportedIn(dataFormat));
        m_DataFormatIn = dataFormat;
        return S_OK;
    }

    virtual HRESULT SetDataFormatOut(MidiDataFormats dataFormat)
    {
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE), !IsFormatSupportedOut(dataFormat));
        m_DataFormatOut = dataFormat;
        return S_OK;
    }

    virtual BOOL IsFormatSupportedIn(MidiDataFormats dataFormat)
    {
        // avoid C26813 warning
        return (BOOL)((m_DataFormatIn & dataFormat) == dataFormat);

        //if (m_DataFormatIn == MidiDataFormats_Any)
        //{
        //    return TRUE;
        //}
        //else
        //{
        //    return m_DataFormatIn == dataFormat;
        //}
    }

    virtual BOOL IsFormatSupportedOut(MidiDataFormats dataFormat)
    {
        // avoid C26813 warning
        return (BOOL)((m_DataFormatOut & dataFormat) == dataFormat);
        
        //if (m_DataFormatOut == MidiDataFormats_Any)
        //{
        //    return TRUE;
        //}
        //else
        //{
        //   return m_DataFormatOut == dataFormat;
        //}
    }

    virtual BOOL InUse()
    {
        auto lock = m_Lock.lock();
        return !m_Clients.empty();
    }

    virtual void AddClient(MidiClientHandle handle)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt64(handle, "MIDI Client Handle"),
            TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        auto lock = m_Lock.lock();
        auto client = std::find(m_Clients.begin(), m_Clients.end(), handle);
        if (client == m_Clients.end())
        {
            m_Clients.push_back(handle);
        }
        else
        {
            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Client already exists", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt64(handle, "MIDI Client Handle"),
                TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );
        }
    }

    virtual void RemoveClient(MidiClientHandle handle)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt64(handle, "MIDI Client Handle"),
            TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        auto lock = m_Lock.lock();
        auto client = std::find(m_Clients.begin(), m_Clients.end(), handle);
        if (client != m_Clients.end())
        {
            m_Clients.erase(client);
        }
        else
        {
            LOG_IF_FAILED(E_NOTFOUND);

            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Client not found", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingUInt64(handle, "MIDI Client Handle"),
                TraceLoggingWideString(m_Device.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );
        }
    }

    // by default, pipes are not group filtered
    virtual BOOL IsGroupFiltered() { return FALSE; }
    virtual BYTE GroupIndex() { return INVALID_GROUP_INDEX; }

private:
    std::wstring m_Device;
    MidiDataFormats m_DataFormatIn{ MidiDataFormats_Invalid };
    MidiDataFormats m_DataFormatOut{ MidiDataFormats_Invalid };
    MidiFlow m_Flow{ MidiFlowIn };

    wil::critical_section m_Lock;
    std::map<MidiPipeHandle, wil::com_ptr_nothrow<CMidiPipe>> m_ConnectedPipes;
    std::vector<MidiClientHandle> m_Clients;

};

