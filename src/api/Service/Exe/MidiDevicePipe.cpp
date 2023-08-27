// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

using namespace winrt::Windows::Devices::Enumeration;

_Use_decl_annotations_
HRESULT
CMidiDevicePipe::Initialize(
    handle_t /* BindingHandle */,
    LPCWSTR Device,
    PMIDISRV_CLIENTCREATION_PARAMS CreationParams,
    DWORD* MmcssTaskId
)
{
    auto clientLock = m_ClientPipeLock.lock();
    auto deviceLock = m_DevicePipeLock.lock();

    m_MidiDevice = wil::make_unique_string<wil::unique_hstring>(Device);
    m_Flow = CreationParams->Flow;
    m_Protocol = CreationParams->Protocol;

    // retrieve the abstraction layer GUID for this peripheral
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_AbstractionLayer));

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(Device, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_AbstractionLayer);
    m_AbstractionGuid = winrt::unbox_value<winrt::guid>(prop);

    if (MidiFlowBidirectional == CreationParams->Flow)
    {
        wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;

        RETURN_IF_FAILED(CoCreateInstance(m_AbstractionGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
        RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&m_MidiBiDiDevice));
        RETURN_IF_FAILED(m_MidiBiDiDevice->Initialize(Device, MmcssTaskId, this));
    }
    else if (MidiFlowIn == CreationParams->Flow)
    {
        wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;

        RETURN_IF_FAILED(CoCreateInstance(m_AbstractionGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
        RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiIn), (void**)&m_MidiInDevice));
        RETURN_IF_FAILED(m_MidiInDevice->Initialize(Device, MmcssTaskId, this));
    }
    else if (MidiFlowOut == CreationParams->Flow)
    {
        wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;

        RETURN_IF_FAILED(CoCreateInstance(m_AbstractionGuid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
        RETURN_IF_FAILED(midiAbstraction->Activate(__uuidof(IMidiOut), (void**)&m_MidiOutDevice));
        RETURN_IF_FAILED(m_MidiOutDevice->Initialize(Device, MmcssTaskId));
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
    {
        auto lock = m_DevicePipeLock.lock();

        if (m_MidiBiDiDevice)
        {
            m_MidiBiDiDevice->Cleanup();
        }
        if (m_MidiInDevice)
        {
            m_MidiInDevice->Cleanup();
        }
        if (m_MidiOutDevice)
        {
            m_MidiOutDevice->Cleanup();
        }

        m_MidiBiDiDevice.reset();
        m_MidiInDevice.reset();
        m_MidiOutDevice.reset();
    }

    {
        auto lock = m_ClientPipeLock.lock();
        m_MidiClientPipes.clear();
    }

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
    // only one client may send a message to the device at a time
    auto lock = m_DevicePipeLock.lock();

    if (m_MidiBiDiDevice)
    {
        return m_MidiBiDiDevice->SendMidiMessage(Data, Length, Position);
    }
    else if (m_MidiOutDevice)
    {
        return m_MidiOutDevice->SendMidiMessage(Data, Length, Position);
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
    // need to hold the client pipe lock to ensure that
    // no clients are added or removed while performing the callback
    // to the client.
    auto lock = m_ClientPipeLock.lock();

    for (auto const& Client : m_MidiClientPipes)
    {
        Client.second->SendMidiMessage(Data, Length, Position);
    }

    return E_ABORT;
}
