// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.ksabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2KSMidiBiDi::Initialize(
    LPCWSTR Device,
    DWORD * MmCss,
    IMidiCallback * Callback
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmCss);

    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, "Device"),
        TraceLoggingHexUInt32(*MmCss, "MmCss Id"),
        TraceLoggingPointer(Callback, "callback")
        );

    // the below KSMidiDeviceEnum is a temporary workaround until we can pass in the SWD to this code
    KSMidiDeviceEnum devices;
    devices.EnumerateFilters();
    wil::unique_handle Filter;    
    UMP_PINS *selectedPinIn = nullptr;
    UMP_PINS *selectedPinOut = nullptr;

    for (UINT i = 0; i < devices.m_AvailableMidiInPinCount; i++)
    {
        if (0 == _wcsicmp(devices.m_AvailableMidiInPins[i].FilterName.get(), Device))
        {
            selectedPinIn = &(devices.m_AvailableMidiInPins[i]);
            break;
        }
    }

    for (UINT i = 0; i < devices.m_AvailableMidiOutPinCount; i++)
    {
        if (0 == _wcsicmp(devices.m_AvailableMidiOutPins[i].FilterName.get(), Device))
        {
            selectedPinOut = &(devices.m_AvailableMidiOutPins[i]);
            break;
        }
    }

    // if no pin was found at all, it's an invalid arg.
    RETURN_HR_IF(E_INVALIDARG, nullptr == selectedPinIn && nullptr == selectedPinOut);

    // if we were able to identify one flow, but not the other, bidirectional
    // is not supported.
    RETURN_HR_IF(E_NOTIMPL, nullptr == selectedPinIn || nullptr == selectedPinOut);

    // bidirectional only applies to UMP, if UMP isn't supported on both pins, bidirectional
    // is not supported for these pins.
    RETURN_HR_IF(E_NOTIMPL, !selectedPinIn->UMP);
    RETURN_HR_IF(E_NOTIMPL, !selectedPinOut->UMP);

    // Both directions need to use the same transfer mechanism
    RETURN_HR_IF(E_NOTIMPL, selectedPinIn->Cyclic != selectedPinOut->Cyclic);

    // the above KSMidiDeviceEnum is a temporary workaround until we can pass in the SWD to this code

    // instantiate both pins on the same filter instance, to better emulate a bidirectional implementation.
    RETURN_IF_FAILED(FilterInstantiate(Device, &Filter));

    // simulate a bidirectional device through using two unidirectional devices, as KS Midi is not currently
    // bidirectional
    std::unique_ptr<KSMidiInDevice> midiInDevice(new (std::nothrow) KSMidiInDevice());
    RETURN_IF_NULL_ALLOC(midiInDevice);

    std::unique_ptr<KSMidiOutDevice> midiOutDevice(new (std::nothrow) KSMidiOutDevice());
    RETURN_IF_NULL_ALLOC(midiOutDevice);

    RETURN_IF_FAILED(midiInDevice->Initialize(Device, Filter.get(), selectedPinIn->PinId, selectedPinIn->Cyclic, TRUE, PAGE_SIZE, MmCss, this));
    m_MidiInCallback = Callback;
    m_MidiInDevice = std::move(midiInDevice);

    RETURN_IF_FAILED(midiOutDevice->Initialize(Device, Filter.get(), selectedPinOut->PinId, selectedPinOut->Cyclic, TRUE, PAGE_SIZE, MmCss));
    m_MidiOutDevice = std::move(midiOutDevice);

    return S_OK;
}

HRESULT
CMidi2KSMidiBiDi::Cleanup()
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
    if (m_MidiInCallback)
    {
        m_MidiInCallback.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidiBiDi::SendMidiMessage(
    PVOID data,
    UINT size,
    LONGLONG position
)
{
    return m_MidiOutDevice->WriteMidiData(data, size, position);
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidiBiDi::Callback(
    PVOID data,
    UINT size,
    LONGLONG position
)
{
    return m_MidiInCallback->Callback(data, size, position);
}

