// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "Midi2.MidiSrv.h"

_Use_decl_annotations_
HRESULT
CMidi2MidiSrv::Initialize(
    LPCWSTR Device,
    MidiFlow Flow,
    PABSTRACTIONCREATIONPARAMS CreationParams,
    DWORD * MmcssTaskId,
    IMidiCallback * Callback,
    LONGLONG Context
)
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    MIDISRV_CLIENTCREATION_PARAMS creationParams{ };
    PMIDISRV_CLIENT client{ nullptr };
    wil::unique_rpc_binding bindingHandle;

    auto cleanupOnError = wil::scope_exit([&]()
    {
        if (m_MidiPump)
        {
            m_MidiPump->Cleanup();
        }

        if (client)
        {
            SAFE_CLOSEHANDLE(client->MidiInDataFileMapping);
            SAFE_CLOSEHANDLE(client->MidiInRegisterFileMapping);
            SAFE_CLOSEHANDLE(client->MidiInWriteEvent);
            SAFE_CLOSEHANDLE(client->MidiOutDataFileMapping);
            SAFE_CLOSEHANDLE(client->MidiOutRegisterFileMapping);
            SAFE_CLOSEHANDLE(client->MidiOutWriteEvent);

            MIDL_user_free(client);
            client = nullptr;
        }
    });

    creationParams.Flow = Flow;
    creationParams.DataFormat = CreationParams->DataFormat;

    // Todo: client side buffering requests to come from some service setting?
    // - See https://github.com/microsoft/MIDI/issues/219 for details
    
    //creationParams.BufferSize = PAGE_SIZE;
    creationParams.BufferSize = 512;    // for debugging DON'T LEAVE THIS IN


    RETURN_IF_FAILED(GetMidiSrvBindingHandle(&bindingHandle));

    RETURN_IF_FAILED([&]()
    {
        // RPC calls are placed in a lambda to work around compiler error C2712, limiting use of try/except blocks
        // with structured exception handling.
        RpcTryExcept RETURN_IF_FAILED(MidiSrvCreateClient(bindingHandle.get(), Device, &creationParams, &client));
        RpcExcept(I_RpcExceptionFilter(RpcExceptionCode())) RETURN_IF_FAILED(HRESULT_FROM_WIN32(RpcExceptionCode()));
        RpcEndExcept
            return S_OK;
    }());

    m_ClientHandle = client->ClientHandle;
    CreationParams->DataFormat = client->DataFormat;

    std::unique_ptr<MEMORY_MAPPED_PIPE> midiInPipe;
    std::unique_ptr<MEMORY_MAPPED_PIPE> midiOutPipe;

    if (Flow == MidiFlowBidirectional || Flow == MidiFlowIn)
    {
        midiInPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE);
        RETURN_IF_NULL_ALLOC(midiInPipe);
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
        midiInPipe->Data.BufferSize = client->MidiInBufferSize;
        RETURN_IF_FAILED(CreateMappedDataBuffer(0, midiInPipe->DataBuffer.get(), &midiInPipe->Data));
        RETURN_IF_FAILED(CreateMappedRegisters(midiInPipe->RegistersBuffer.get(), &midiInPipe->Registers));
    }

    if (Flow == MidiFlowBidirectional || Flow == MidiFlowOut)
    {
        midiOutPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE);
        RETURN_IF_NULL_ALLOC(midiOutPipe);
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
    RETURN_IF_FAILED(m_MidiPump->Initialize(MmcssTaskId, midiInPipe, midiOutPipe, Callback, Context, false));

    cleanupOnError.release();

    return S_OK;
}

HRESULT
CMidi2MidiSrv::Cleanup()
{
    TraceLoggingWrite(
        MidiSrvAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (m_MidiPump)
    {
        RETURN_IF_FAILED(m_MidiPump->Cleanup());
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
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    if (m_MidiPump)
    {
        return m_MidiPump->SendMidiMessage(Data, Length, Position);;
    }

    return E_ABORT;
}

