// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
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
    handle_t /* BindingHandle */,
    HANDLE /* clientProcess */,
    LPCWSTR Device,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    PMIDISRV_CLIENT Client,
    DWORD* MmcssTaskId,
    wil::com_ptr_nothrow<CMidiDevicePipe>& DevicePipe
)
{
    std::unique_ptr<MEMORY_MAPPED_PIPE> midiInPipe;
    std::unique_ptr<MEMORY_MAPPED_PIPE> midiOutPipe;
    wil::com_ptr_nothrow<IMidiCallback> thisCallback;

    m_ClientPipe.MidiDevice = Device;
    m_ClientPipe.Flow = CreationParams->Flow;
    m_ClientPipe.Protocol = CreationParams->Protocol;

    m_MidiDevicePipe = DevicePipe;

    auto cleanupOnFailure = wil::scope_exit([&]()
    {
        SAFE_CLOSEHANDLE(Client->MidiInDataFileMapping);
        SAFE_CLOSEHANDLE(Client->MidiInRegisterFileMapping);
        SAFE_CLOSEHANDLE(Client->MidiInWriteEvent);
        SAFE_CLOSEHANDLE(Client->MidiOutDataFileMapping);
        SAFE_CLOSEHANDLE(Client->MidiOutRegisterFileMapping);
        SAFE_CLOSEHANDLE(Client->MidiOutWriteEvent);
    });

    if (m_ClientPipe.Flow == MidiFlowBidirectional ||
        m_ClientPipe.Flow == MidiFlowIn)
    {
        midiInPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE );
        RETURN_IF_NULL_ALLOC(midiInPipe);

        midiInPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiInPipe->DataBuffer);

        midiInPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiInPipe->RegistersBuffer);

        // Midi in controls, buffering, and eventing.
        RETURN_IF_FAILED(CreateMappedDataBuffer(CreationParams->BufferSize, midiInPipe->DataBuffer.get(), &midiInPipe->Data));
        RETURN_IF_FAILED(CreateMappedRegisters(midiInPipe->RegistersBuffer.get(), &midiInPipe->Registers));
        midiInPipe->WriteEvent.create(wil::EventOptions::ManualReset);

        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->DataBuffer->FileMapping.get(), GetCurrentProcess(), &(Client->MidiInDataFileMapping), DUPLICATE_SAME_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->RegistersBuffer->FileMapping.get(), GetCurrentProcess(), &(Client->MidiInRegisterFileMapping), DUPLICATE_SAME_ACCESS, TRUE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiInPipe->WriteEvent.get(), GetCurrentProcess(), &(Client->MidiInWriteEvent), EVENT_ALL_ACCESS, TRUE, 0));
        Client->MidiInBufferSize = midiInPipe->Data.BufferSize;
    }

    if (m_ClientPipe.Flow == MidiFlowBidirectional ||
        m_ClientPipe.Flow == MidiFlowOut)
    {
        midiOutPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE );
        RETURN_IF_NULL_ALLOC(midiOutPipe);

        midiOutPipe->DataBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiOutPipe->DataBuffer);

        midiOutPipe->RegistersBuffer.reset(new (std::nothrow) MEMORY_MAPPED_BUFFER);
        RETURN_IF_NULL_ALLOC(midiOutPipe->RegistersBuffer);

        // Midi out controls, buffering, and eventing
        RETURN_IF_FAILED(CreateMappedDataBuffer(CreationParams->BufferSize, midiOutPipe->DataBuffer.get(), &midiOutPipe->Data));
        RETURN_IF_FAILED(CreateMappedRegisters(midiOutPipe->RegistersBuffer.get(), &midiOutPipe->Registers));
        midiOutPipe->WriteEvent.create(wil::EventOptions::ManualReset);

        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->DataBuffer->FileMapping.get(), GetCurrentProcess(), &(Client->MidiOutDataFileMapping), DUPLICATE_SAME_ACCESS, FALSE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->RegistersBuffer->FileMapping.get(), GetCurrentProcess(), &(Client->MidiOutRegisterFileMapping), DUPLICATE_SAME_ACCESS, FALSE, 0));
        RETURN_LAST_ERROR_IF(FALSE == DuplicateHandle(GetCurrentProcess(), midiOutPipe->WriteEvent.get(), GetCurrentProcess(), &(Client->MidiOutWriteEvent), EVENT_ALL_ACCESS, TRUE, 0));
        Client->MidiOutBufferSize = midiOutPipe->Data.BufferSize;
    }

    m_ClientPipe.MidiPump.reset(new (std::nothrow) CMidiXProc());
    RETURN_IF_NULL_ALLOC(m_ClientPipe.MidiPump);

    RETURN_IF_FAILED(QueryInterface(__uuidof(IMidiCallback), (void**)&thisCallback));

    // Adding the client pipe to the device pipe creates an intentional circular reference between the client
    // and device. After this point, a proper cleanup is required, simply releasing the two will, intentionally, not
    // clean up because there is a cross dependency between the client pipe and device pipe that requires a proper
    // shutdown.
    wil::com_ptr_nothrow<CMidiClientPipe> This = this;
    RETURN_IF_FAILED(m_MidiDevicePipe->AddClientPipe(This));

    // The client MidiOut pipe is an input from the client side, sending data to the server,
    // and the client MidiIn pipe is an output from the server, sending data to the client.
    // Thus, the pump is initialized with midiOutPipe for input and midiInPipe for output,
    // which appears backwards if you inspect the function prototype, but is correct for the connection
    // from the client to the server.
    RETURN_IF_FAILED(m_ClientPipe.MidiPump->Initialize(MmcssTaskId, midiOutPipe, midiInPipe, thisCallback.get()));

    cleanupOnFailure.release();

    return S_OK;
}

HRESULT
CMidiClientPipe::Cleanup()
{
    if (m_ClientPipe.MidiPump)
    {
        m_ClientPipe.MidiPump->Cleanup();
        m_ClientPipe.MidiPump.reset();
    }

    wil::com_ptr_nothrow<CMidiClientPipe> This = this;
    m_MidiDevicePipe->RemoveClientPipe(This);
    m_MidiDevicePipe.reset();

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
    if (m_ClientPipe.MidiPump)
    {
        return m_ClientPipe.MidiPump->SendMidiMessage(Data, Length, Position);
    }
    return E_ABORT;
}

_Use_decl_annotations_
HRESULT
CMidiClientPipe::Callback(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    if (m_MidiDevicePipe)
    {
        return m_MidiDevicePipe->SendMidiMessage(Data, Length, Position);
    }
    return E_ABORT;
}
