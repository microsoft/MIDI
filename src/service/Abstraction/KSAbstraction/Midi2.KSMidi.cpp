// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.ksabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2KSMidi::Initialize(
    LPCWSTR Device,
    BOOL MidiIn,
    BOOL MidiOut,
    DWORD * MmCssTaskId,
    IMidiCallback * Callback
)
{
    RETURN_HR_IF(E_INVALIDARG, MidiIn && nullptr == Callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmCssTaskId);
    RETURN_HR_IF(E_INVALIDARG, !MidiIn && !MidiOut);

    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, "Device"),
        TraceLoggingBool(MidiIn, "MidiIn"),
        TraceLoggingBool(MidiOut, "MidiOut"),
        TraceLoggingHexUInt32(*MmCssTaskId, "MmCssTaskId"),
        TraceLoggingPointer(Callback, "callback")
        );

    // the below KSMidiDeviceEnum is a temporary workaround until we can pass in the SWD to this code
    KSMidiDeviceEnum devices;
    devices.EnumerateFilters();
    wil::unique_handle Filter;    
    UMP_PINS *selectedPinIn = nullptr;
    UMP_PINS *selectedPinOut = nullptr;
    std::unique_ptr<KSMidiInDevice> midiInDevice;
    std::unique_ptr<KSMidiOutDevice> midiOutDevice;

    // Todo: buffer size and other device parameters (cyclic/legacy) shall come from the SWD,
    // for now use the system allocation granularity and assume cyclic.
    ULONG requestedBufferSize = PAGE_SIZE;
    RETURN_IF_FAILED(GetRequiredBufferSize(requestedBufferSize));

    if (MidiIn)
    {
        for (UINT i = 0; i < devices.m_AvailableMidiInPinCount; i++)
        {
            if (0 == _wcsicmp(devices.m_AvailableMidiInPins[i].FilterName.get(), Device))
            {
                selectedPinIn = &(devices.m_AvailableMidiInPins[i]);
                break;
            }
        }
    }

    if (MidiOut)
    {
        for (UINT i = 0; i < devices.m_AvailableMidiOutPinCount; i++)
        {
            if (0 == _wcsicmp(devices.m_AvailableMidiOutPins[i].FilterName.get(), Device))
            {
                selectedPinOut = &(devices.m_AvailableMidiOutPins[i]);
                break;
            }
        }
    }

    RETURN_HR_IF(E_INVALIDARG, MidiIn && nullptr == selectedPinIn);
    RETURN_HR_IF(E_INVALIDARG, MidiOut && nullptr == selectedPinOut);

    // bidirectional only applies to UMP, if UMP isn't supported on both pins, bidirectional
    // is not supported for these pins.
    if (MidiIn && MidiOut)
    {
        RETURN_HR_IF(E_NOTIMPL, !selectedPinIn->UMP);
        RETURN_HR_IF(E_NOTIMPL, !selectedPinOut->UMP);

        // Both directions need to use the same transfer mechanism
        RETURN_HR_IF(E_NOTIMPL, selectedPinIn->Cyclic != selectedPinOut->Cyclic);
    }
    // the above KSMidiDeviceEnum is a temporary workaround until we can pass in the SWD to this code

    // instantiate both pins on the same filter instance, to better emulate a bidirectional implementation.
    RETURN_IF_FAILED(FilterInstantiate(Device, &Filter));

    // simulate a bidirectional device through using two unidirectional devices, as KS Midi is not currently
    // bidirectional
    if (MidiIn)
    {
        midiInDevice.reset(new (std::nothrow) KSMidiInDevice());
        RETURN_IF_NULL_ALLOC(midiInDevice);
        RETURN_IF_FAILED(midiInDevice->Initialize(Device, Filter.get(), selectedPinIn->PinId, selectedPinIn->Cyclic, TRUE, requestedBufferSize, MmCssTaskId, Callback));
        m_MidiInDevice = std::move(midiInDevice);
    }

    if (MidiOut)
    {
        midiOutDevice.reset(new (std::nothrow) KSMidiOutDevice());
        RETURN_IF_NULL_ALLOC(midiOutDevice);
        RETURN_IF_FAILED(midiOutDevice->Initialize(Device, Filter.get(), selectedPinOut->PinId, selectedPinOut->Cyclic, TRUE, requestedBufferSize, MmCssTaskId));
        m_MidiOutDevice = std::move(midiOutDevice);
    }

    return S_OK;
}

HRESULT
CMidi2KSMidi::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (m_MidiOutDevice)
    {
        m_MidiOutDevice->Cleanup();
        m_MidiOutDevice.reset();
    }
    if (m_MidiInDevice)
    {
        m_MidiInDevice->Cleanup();
        m_MidiInDevice.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidi::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    if (m_MidiOutDevice)
    {
        return m_MidiOutDevice->SendMidiMessage(Data, Length, Position);
    }

    return E_ABORT;
}

