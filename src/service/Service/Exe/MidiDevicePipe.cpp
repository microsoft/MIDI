// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

_Use_decl_annotations_
HRESULT
CMidiDevicePipe::Initialize(
    handle_t /* BindingHandle */,
    HANDLE /* clientProcess */,
    LPCWSTR Device,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    DWORD* MmcssTaskId
)
{
    m_MidiDevice = wil::make_unique_string<wil::unique_hstring>(Device);
    m_Flow = CreationParams->Flow;
    m_Protocol = CreationParams->Protocol;

    // Todo: Currently hard coded to create the KS abstraction. Long term this needs to retrieve
    // the device from the midi device manager, determine what abstraction is required, and cocreate the
    // abstraction related to the requested device, rather than assuming KS.

    if (MidiFlowBidirectional == CreationParams->Flow)
    {
        wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;

        RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2KSAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
        RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&m_KSMidiBiDiDevice));
        RETURN_IF_FAILED(m_KSMidiBiDiDevice->Initialize(Device, MmcssTaskId, this));
    }
    else if (MidiFlowIn == CreationParams->Flow)
    {
        wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;

        RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2KSAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
        RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiIn), (void**)&m_KSMidiInDevice));
        RETURN_IF_FAILED(m_KSMidiInDevice->Initialize(Device, MmcssTaskId, this));
    }
    else if (MidiFlowOut == CreationParams->Flow)
    {
        wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;

        RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2KSAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
        RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiOut), (void**)&m_KSMidiOutDevice));
        RETURN_IF_FAILED(m_KSMidiOutDevice->Initialize(Device, MmcssTaskId));
    }
    else
    {
        return E_INVALIDARG;
    }

    return S_OK;
}

HRESULT
CMidiDevicePipe::Cleanup()
{
    if (m_KSMidiBiDiDevice)
    {
        m_KSMidiBiDiDevice->Cleanup();
    }
    if (m_KSMidiInDevice)
    {
        m_KSMidiInDevice->Cleanup();
    }
    if (m_KSMidiOutDevice)
    {
        m_KSMidiOutDevice->Cleanup();
    }

    m_KSMidiBiDiDevice.reset();
    m_KSMidiInDevice.reset();
    m_KSMidiOutDevice.reset();

    m_MidiClientPipe.reset();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiDevicePipe::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    if (m_KSMidiBiDiDevice)
    {
        return m_KSMidiBiDiDevice->SendMidiMessage(Data, Length, Position);
    }
    else if (m_KSMidiOutDevice)
    {
        return m_KSMidiOutDevice->SendMidiMessage(Data, Length, Position);
    }

    return E_ABORT;
}

_Use_decl_annotations_
HRESULT
CMidiDevicePipe::Callback(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    if (m_MidiClientPipe)
    {
        return m_MidiClientPipe->SendMidiMessage(Data, Length, Position);
    }
    return E_ABORT;
}
