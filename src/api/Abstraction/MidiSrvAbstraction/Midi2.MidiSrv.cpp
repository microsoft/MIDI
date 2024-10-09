// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "Midi2.MidiSrv.h"

_Use_decl_annotations_
HRESULT
CMidi2MidiSrv::Initialize(
    LPCWSTR device,
    MidiFlow flow,
    PABSTRACTIONCREATIONPARAMS creationParams,
    DWORD * mmcssTaskId,
    IMidiCallback * callback,
    LONGLONG context,
    GUID sessionId
)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    MIDISRV_CLIENTCREATION_PARAMS clientCreationParams{ };
    PMIDISRV_CLIENT client{ nullptr };
    wil::unique_rpc_binding bindingHandle;

    auto cleanupOnError = wil::scope_exit([&]()
    {
        if (m_MidiPump)
        {
            m_MidiPump->Shutdown();
        }

        if (client)
        {
            SAFE_CLOSEHANDLE(client->MidiInDataFileMapping);
            SAFE_CLOSEHANDLE(client->MidiInRegisterFileMapping);
            SAFE_CLOSEHANDLE(client->MidiInWriteEvent);
            SAFE_CLOSEHANDLE(client->MidiInReadEvent);
            SAFE_CLOSEHANDLE(client->MidiOutDataFileMapping);
            SAFE_CLOSEHANDLE(client->MidiOutRegisterFileMapping);
            SAFE_CLOSEHANDLE(client->MidiOutWriteEvent);
            SAFE_CLOSEHANDLE(client->MidiOutReadEvent);

            MIDL_user_free(client);
            client = nullptr;
        }
    });

    clientCreationParams.Flow = flow;
    clientCreationParams.DataFormat = creationParams->DataFormat;

    // Todo: client side buffering requests to come from some service setting?
    // - See https://aka.ms/midiissues/219 for details
    
    //clientCreationParams.BufferSize = PAGE_SIZE;  // original
    //clientCreationParams.BufferSize = 512;    // Set this for debugging see https://aka.ms/midiissues/182 for all the drama :)
    clientCreationParams.BufferSize = PAGE_SIZE * 2;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));

    RETURN_IF_FAILED([&]()
    {
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvCreateClient(bindingHandle.get(), device, &clientCreationParams, sessionId, &client));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
        return S_OK;
    }());

    m_ClientHandle = client->ClientHandle;
    creationParams->DataFormat = client->DataFormat;

    std::unique_ptr<MEMORY_MAPPED_PIPE> midiInPipe;
    std::unique_ptr<MEMORY_MAPPED_PIPE> midiOutPipe;

    if (flow == MidiFlowBidirectional || flow == MidiFlowIn)
    {
        midiInPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE);
        RETURN_IF_NULL_ALLOC(midiInPipe);
        midiInPipe->DataFormat = client->DataFormat;
        midiInPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiInPipe->DataBuffer);
        midiInPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiInPipe->RegistersBuffer);
        midiInPipe->DataBuffer->FileMapping.reset(client->MidiInDataFileMapping);
        client->MidiInDataFileMapping = NULL;
        midiInPipe->RegistersBuffer->FileMapping.reset(client->MidiInRegisterFileMapping);
        client->MidiInRegisterFileMapping = NULL;
        midiInPipe->WriteEvent.reset(client->MidiInWriteEvent);
        client->MidiInWriteEvent = NULL;
        midiInPipe->ReadEvent.reset(client->MidiInReadEvent);
        client->MidiInReadEvent = NULL;
        midiInPipe->Data.BufferSize = client->MidiInBufferSize;
        RETURN_IF_FAILED(CreateMappedDataBuffer(0, midiInPipe->DataBuffer.get(), &midiInPipe->Data));
        RETURN_IF_FAILED(CreateMappedRegisters(midiInPipe->RegistersBuffer.get(), &midiInPipe->Registers));
    }

    if (flow == MidiFlowBidirectional || flow == MidiFlowOut)
    {
        midiOutPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE);
        RETURN_IF_NULL_ALLOC(midiOutPipe);
        midiOutPipe->DataFormat = client->DataFormat;
        midiOutPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiOutPipe->DataBuffer);
        midiOutPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiOutPipe->RegistersBuffer);
        midiOutPipe->DataBuffer->FileMapping.reset(client->MidiOutDataFileMapping);
        client->MidiOutDataFileMapping = NULL;
        midiOutPipe->RegistersBuffer->FileMapping.reset(client->MidiOutRegisterFileMapping);
        client->MidiOutRegisterFileMapping = NULL;
        midiOutPipe->WriteEvent.reset(client->MidiOutWriteEvent);
        client->MidiOutWriteEvent = NULL;
        midiOutPipe->ReadEvent.reset(client->MidiOutReadEvent);
        client->MidiOutReadEvent = NULL;
        midiOutPipe->Data.BufferSize = client->MidiOutBufferSize;
        // Midi out controls, buffering, and eventing
        RETURN_IF_FAILED(CreateMappedDataBuffer(0, midiOutPipe->DataBuffer.get(), &midiOutPipe->Data));
        RETURN_IF_FAILED(CreateMappedRegisters(midiOutPipe->RegistersBuffer.get(), &midiOutPipe->Registers));
    }

    MIDL_user_free(client);
    client = nullptr;

    m_MidiPump.reset(new (std::nothrow) CMidiXProc());
    RETURN_IF_NULL_ALLOC(m_MidiPump);

    // TODO: Need to double-check if the "false" for auto-generating the timestamp is appropriate here.
    RETURN_IF_FAILED(m_MidiPump->Initialize(mmcssTaskId, midiInPipe, midiOutPipe, callback, context, false));

    cleanupOnError.release();

    return S_OK;
}

HRESULT
CMidi2MidiSrv::Shutdown()
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (m_MidiPump)
    {
        RETURN_IF_FAILED(m_MidiPump->Shutdown());
        m_MidiPump.reset();
    }

    wil::unique_rpc_binding bindingHandle;

    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));

    if (0 != m_ClientHandle)
    {
        RETURN_IF_FAILED([&]()
        {
            // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
            // with structured exception handling.
            RpcTryExcept RETURN_IF_FAILED(MidiSrvDestroyClient(bindingHandle.get(), m_ClientHandle));
            RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
            RpcEndExcept
                return S_OK;
        }());

        m_ClientHandle = 0;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrv::SendMidiMessage(
    PVOID data,
    UINT length,
    LONGLONG position
)
{
    if (m_MidiPump)
    {
        return m_MidiPump->SendMidiMessage(data, length, position);;
    }

    return E_ABORT;
}

