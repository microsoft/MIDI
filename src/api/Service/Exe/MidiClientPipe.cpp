// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

_Use_decl_annotations_
HRESULT
CMidiClientPipe::AdjustForBufferingRequirements(PMIDISRV_CLIENTCREATION_PARAMS CreationParams
)
{
    RETURN_IF_FAILED(GetRequiredBufferSize(CreationParams->BufferSize));
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiClientPipe::Initialize(
    handle_t /* BindingHandle */,   // this isn't used now, but if used in the future, not this will not be set for internal clients like protocol negotiation
    HANDLE /* clientProcess */,     // this isn't used now, but if used in the future, not this will not be set for internal clients like protocol negotiation
    LPCWSTR Device,
    GUID SessionId,
    DWORD ClientProcessId,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    PMIDISRV_CLIENT Client,
    DWORD* MmcssTaskId,
    BOOL OverwriteZeroTimestamps
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingGuid(SessionId),
        TraceLoggingUInt32(ClientProcessId, "Client Process")
    );

    // for tracking the client connection
    m_sessionId = SessionId;
    m_clientProcessId = ClientProcessId;
    //m_device = internal::NormalizeEndpointInterfaceIdCopy(Device);


    auto lock = m_ClientPipeLock.lock();

    std::unique_ptr<MEMORY_MAPPED_PIPE> midiInPipe;
    std::unique_ptr<MEMORY_MAPPED_PIPE> midiOutPipe;
    wil::com_ptr_nothrow<IMidiCallback> thisCallback;

    RETURN_IF_FAILED(CMidiPipe::Initialize(Device, CreationParams->Flow));

    RETURN_IF_FAILED(AdjustForBufferingRequirements(CreationParams));

    // Save the data format(s) for this client
    if (IsFlowSupported(MidiFlowIn))
    {
        RETURN_IF_FAILED(SetDataFormatIn(CreationParams->DataFormat));
    }

    if (IsFlowSupported(MidiFlowOut))
    {
        RETURN_IF_FAILED(SetDataFormatOut(CreationParams->DataFormat));
    }

    auto cleanupOnFailure = wil::scope_exit([&]()
    {
        SAFE_CLOSEHANDLE(Client->MidiInDataFileMapping);
        SAFE_CLOSEHANDLE(Client->MidiInRegisterFileMapping);
        SAFE_CLOSEHANDLE(Client->MidiInWriteEvent);
        SAFE_CLOSEHANDLE(Client->MidiInReadEvent);
        SAFE_CLOSEHANDLE(Client->MidiOutDataFileMapping);
        SAFE_CLOSEHANDLE(Client->MidiOutRegisterFileMapping);
        SAFE_CLOSEHANDLE(Client->MidiOutWriteEvent);
        SAFE_CLOSEHANDLE(Client->MidiOutReadEvent);
    });

    if (IsFlowSupported(MidiFlowIn))
    {
        midiInPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE );
        RETURN_IF_NULL_ALLOC(midiInPipe);

        midiInPipe->DataFormat = CreationParams->DataFormat;

        midiInPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiInPipe->DataBuffer);

        midiInPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiInPipe->RegistersBuffer);

        // Midi in controls, buffering, and eventing.
        RETURN_IF_FAILED(CreateMappedDataBuffer(CreationParams->BufferSize, midiInPipe->DataBuffer.get(), &midiInPipe->Data));
        RETURN_IF_FAILED(CreateMappedRegisters(midiInPipe->RegistersBuffer.get(), &midiInPipe->Registers));
        midiInPipe->WriteEvent.create(wil::EventOptions::ManualReset);
        midiInPipe->ReadEvent.create(wil::EventOptions::ManualReset);

        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->DataBuffer->FileMapping.get(), GetCurrentProcess(), &(Client->MidiInDataFileMapping), DUPLICATE_SAME_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->RegistersBuffer->FileMapping.get(), GetCurrentProcess(), &(Client->MidiInRegisterFileMapping), DUPLICATE_SAME_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->WriteEvent.get(), GetCurrentProcess(), &(Client->MidiInWriteEvent), EVENT_ALL_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->ReadEvent.get(), GetCurrentProcess(), &(Client->MidiInReadEvent), EVENT_ALL_ACCESS, TRUE, 0));
        Client->MidiInBufferSize = midiInPipe->Data.BufferSize;
    }

    if (IsFlowSupported(MidiFlowOut))
    {
        midiOutPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE );
        RETURN_IF_NULL_ALLOC(midiOutPipe);

        midiOutPipe->DataFormat = CreationParams->DataFormat;

        midiOutPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiOutPipe->DataBuffer);

        midiOutPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiOutPipe->RegistersBuffer);

        // Midi out controls, buffering, and eventing
        RETURN_IF_FAILED(CreateMappedDataBuffer(CreationParams->BufferSize, midiOutPipe->DataBuffer.get(), &midiOutPipe->Data));
        RETURN_IF_FAILED(CreateMappedRegisters(midiOutPipe->RegistersBuffer.get(), &midiOutPipe->Registers));
        midiOutPipe->WriteEvent.create(wil::EventOptions::ManualReset);
        midiOutPipe->ReadEvent.create(wil::EventOptions::ManualReset);

        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->DataBuffer->FileMapping.get(), GetCurrentProcess(), &(Client->MidiOutDataFileMapping), DUPLICATE_SAME_ACCESS, FALSE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->RegistersBuffer->FileMapping.get(), GetCurrentProcess(), &(Client->MidiOutRegisterFileMapping), DUPLICATE_SAME_ACCESS, FALSE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->WriteEvent.get(), GetCurrentProcess(), &(Client->MidiOutWriteEvent), EVENT_ALL_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->ReadEvent.get(), GetCurrentProcess(), &(Client->MidiOutReadEvent), EVENT_ALL_ACCESS, TRUE, 0));
        Client->MidiOutBufferSize = midiOutPipe->Data.BufferSize;
    }

    m_MidiPump.reset(new (std::nothrow) CMidiXProc());
    RETURN_IF_NULL_ALLOC(m_MidiPump);

    RETURN_IF_FAILED(QueryInterface(__uuidof(IMidiCallback), (void**)&thisCallback));

    // The client MidiOut pipe is an input from the client side, sending data to the server,
    // and the client MidiIn pipe is an output from the server, sending data to the client.
    // Thus, the pump is initialized with midiOutPipe for input and midiInPipe for output,
    // which appears backwards if you inspect the function prototype, but is correct for the connection
    // from the client to the server.
    RETURN_IF_FAILED(m_MidiPump->Initialize(MmcssTaskId, midiOutPipe, midiInPipe, thisCallback.get(), 0, OverwriteZeroTimestamps));

    cleanupOnFailure.release();

    return S_OK;
}

HRESULT
CMidiClientPipe::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    auto lock = m_ClientPipeLock.lock();
    if (m_MidiPump)
    {
        m_MidiPump->Cleanup();
        m_MidiPump.reset();
    }

    RETURN_IF_FAILED(CMidiPipe::Cleanup());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiClientPipe::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    auto lock = m_ClientPipeLock.lock();
    if (m_MidiPump)
    {
        return m_MidiPump->SendMidiMessage(Data, Length, Position);
    }
    return E_ABORT;
}

_Use_decl_annotations_
HRESULT
CMidiClientPipe::SendMidiMessageNow(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    auto lock = m_ClientPipeLock.lock();
    if (m_MidiPump)
    {
        // TODO: add a SendMidiMessageNow routine to the abstraction layers.
        return m_MidiPump->SendMidiMessage(Data, Length, Position);
    }
    return E_ABORT;
}

