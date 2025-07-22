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
CMidiClientPipe::AdjustForBufferingRequirements(PMIDISRV_CLIENTCREATION_PARAMS creationParams
)
{
    RETURN_IF_FAILED(GetRequiredBufferSize(creationParams->BufferSize));
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiClientPipe::Initialize(
    HANDLE /* clientProcess */,     // this isn't used now, but if used in the future, not this will not be set for internal clients like protocol negotiation
    LPCWSTR device,
    BYTE groupIndex,
    GUID sessionId,
    DWORD clientProcessId,
    PMIDISRV_CLIENTCREATION_PARAMS creationParams,
    PMIDISRV_CLIENT client,
    DWORD* mmcssTaskId,
    BOOL overwriteZeroTimestamps
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(device, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingGuid(sessionId),
        TraceLoggingUInt32(clientProcessId, "Client Process")
    );

    // for tracking the client connection
    m_sessionId = sessionId;
    m_clientProcessId = clientProcessId;
    //m_device = internal::NormalizeEndpointInterfaceIdCopy(device);

    // If this is a bytestream client and we were provided a valid group
    // index, then enable filtering messages to that group.
    if (creationParams->DataFormat == MidiDataFormats_ByteStream && 
        IS_VALID_GROUP_INDEX(groupIndex))
    {
        m_GroupIndex = groupIndex;
        m_GroupFiltered = TRUE;
    }

    auto lock = m_ClientPipeLock.lock_exclusive();

    std::unique_ptr<MEMORY_MAPPED_PIPE> midiInPipe;
    std::unique_ptr<MEMORY_MAPPED_PIPE> midiOutPipe;
    wil::com_ptr_nothrow<IMidiCallback> thisCallback;

    RETURN_IF_FAILED(CMidiPipe::Initialize(device, creationParams->Flow));

    RETURN_IF_FAILED(AdjustForBufferingRequirements(creationParams));

    // Save the data format(s) for this client
    if (IsFlowSupported(MidiFlowIn))
    {
        RETURN_IF_FAILED(SetDataFormatIn(creationParams->DataFormat));
    }

    if (IsFlowSupported(MidiFlowOut))
    {
        RETURN_IF_FAILED(SetDataFormatOut(creationParams->DataFormat));
    }

    auto cleanupOnFailure = wil::scope_exit([&]()
    {
        SAFE_CLOSEHANDLE(client->MidiInDataFileMapping);
        SAFE_CLOSEHANDLE(client->MidiInRegisterFileMapping);
        SAFE_CLOSEHANDLE(client->MidiInWriteEvent);
        SAFE_CLOSEHANDLE(client->MidiInReadEvent);
        SAFE_CLOSEHANDLE(client->MidiOutDataFileMapping);
        SAFE_CLOSEHANDLE(client->MidiOutRegisterFileMapping);
        SAFE_CLOSEHANDLE(client->MidiOutWriteEvent);
        SAFE_CLOSEHANDLE(client->MidiOutReadEvent);
    });

    if (IsFlowSupported(MidiFlowIn))
    {
        GUID generatedName;
        wil::unique_cotaskmem_string readEventName;

        midiInPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE );
        RETURN_IF_NULL_ALLOC(midiInPipe);

        midiInPipe->DataFormat = creationParams->DataFormat;

        midiInPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiInPipe->DataBuffer);

        midiInPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiInPipe->RegistersBuffer);

        // Midi in controls, buffering, and eventing.
        RETURN_IF_FAILED(CreateMappedDataBuffer(creationParams->BufferSize, midiInPipe->DataBuffer.get(), &midiInPipe->Data));
        RETURN_IF_FAILED(CreateMappedRegisters(midiInPipe->RegistersBuffer.get(), &midiInPipe->Registers));
        midiInPipe->WriteEvent.create(wil::EventOptions::ManualReset);

        // generate a unique name for the read event
        RETURN_IF_FAILED(CoCreateGuid(&generatedName));
        RETURN_IF_FAILED(StringFromCLSID(generatedName, &readEventName));

        midiInPipe->ReadEventName = L"Global\\";
        midiInPipe->ReadEventName += readEventName.get();
        midiInPipe->ReadEvent.reset(CreateEvent(NULL, TRUE, FALSE, midiInPipe->ReadEventName.c_str()));

        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->DataBuffer->FileMapping.get(), GetCurrentProcess(), &(client->MidiInDataFileMapping), DUPLICATE_SAME_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->RegistersBuffer->FileMapping.get(), GetCurrentProcess(), &(client->MidiInRegisterFileMapping), DUPLICATE_SAME_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->WriteEvent.get(), GetCurrentProcess(), &(client->MidiInWriteEvent), EVENT_ALL_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->ReadEvent.get(), GetCurrentProcess(), &(client->MidiInReadEvent), EVENT_ALL_ACCESS, TRUE, 0));
        client->MidiInBufferSize = midiInPipe->Data.BufferSize;
    }

    if (IsFlowSupported(MidiFlowOut))
    {
        GUID generatedName;
        wil::unique_cotaskmem_string readEventName;

        midiOutPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE );
        RETURN_IF_NULL_ALLOC(midiOutPipe);

        midiOutPipe->DataFormat = creationParams->DataFormat;

        midiOutPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiOutPipe->DataBuffer);

        midiOutPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiOutPipe->RegistersBuffer);

        // Midi out controls, buffering, and eventing
        RETURN_IF_FAILED(CreateMappedDataBuffer(creationParams->BufferSize, midiOutPipe->DataBuffer.get(), &midiOutPipe->Data));
        RETURN_IF_FAILED(CreateMappedRegisters(midiOutPipe->RegistersBuffer.get(), &midiOutPipe->Registers));
        midiOutPipe->WriteEvent.create(wil::EventOptions::ManualReset);

        // generate a unique name for the read event
        RETURN_IF_FAILED(CoCreateGuid(&generatedName));
        RETURN_IF_FAILED(StringFromCLSID(generatedName, &readEventName));

        midiOutPipe->ReadEventName = L"Global\\";
        midiOutPipe->ReadEventName += readEventName.get();
        midiOutPipe->ReadEvent.reset(CreateEvent(NULL, TRUE, FALSE, midiOutPipe->ReadEventName.c_str()));

        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->DataBuffer->FileMapping.get(), GetCurrentProcess(), &(client->MidiOutDataFileMapping), DUPLICATE_SAME_ACCESS, FALSE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->RegistersBuffer->FileMapping.get(), GetCurrentProcess(), &(client->MidiOutRegisterFileMapping), DUPLICATE_SAME_ACCESS, FALSE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->WriteEvent.get(), GetCurrentProcess(), &(client->MidiOutWriteEvent), EVENT_ALL_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->ReadEvent.get(), GetCurrentProcess(), &(client->MidiOutReadEvent), EVENT_ALL_ACCESS, TRUE, 0));
        client->MidiOutBufferSize = midiOutPipe->Data.BufferSize;
    }

    m_MidiPump.reset(new (std::nothrow) CMidiXProc());
    RETURN_IF_NULL_ALLOC(m_MidiPump);

    RETURN_IF_FAILED(QueryInterface(__uuidof(IMidiCallback), (void**)&thisCallback));

    // The client MidiOut pipe is an input from the client side, sending data to the server,
    // and the client MidiIn pipe is an output from the server, sending data to the client.
    // Thus, the pump is initialized with midiOutPipe for input and midiInPipe for output,
    // which appears backwards if you inspect the function prototype, but is correct for the connection
    // from the client to the server.
    // Set our callback context to our group index (if supplied), so proper filtering is applied.
    RETURN_IF_FAILED(m_MidiPump->Initialize(mmcssTaskId, midiOutPipe, midiInPipe, thisCallback.get(), groupIndex, overwriteZeroTimestamps));

    // Additional message options requested by the client process
    m_MessageOptions = creationParams->MessageOptions;

    cleanupOnFailure.release();

    return S_OK;
}

HRESULT
CMidiClientPipe::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    auto lock = m_ClientPipeLock.lock_exclusive();

    if (m_MidiPump)
    {
        m_MidiPump->Shutdown();
        m_MidiPump.reset();
    }

    RETURN_IF_FAILED(CMidiPipe::Shutdown());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiClientPipe::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT length,
    LONGLONG position
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Sending MIDI Message to client.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        //TraceLoggingHexUInt8Array(static_cast<uint8_t*>(data), static_cast<uint16_t>(length), "data"),
        TraceLoggingPointer(data, "data"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );

    auto lock = m_ClientPipeLock.lock_shared();

    RETURN_HR_IF_NULL(E_ABORT, m_MidiPump);   
    RETURN_IF_FAILED(m_MidiPump->SendMidiMessage((MessageOptionFlags) (m_MessageOptions | optionFlags), data, length, position));

    return S_OK;
}

