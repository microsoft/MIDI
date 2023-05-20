// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.ksabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2KSMidiIn::Initialize(
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
    UMP_PINS *selectedPin = nullptr;

    for (UINT i = 0; i < devices.m_AvailableMidiInPinCount; i++)
    {
        if (0 == _wcsicmp(devices.m_AvailableMidiInPins[i].FilterName.get(), Device))
        {
            selectedPin = &(devices.m_AvailableMidiInPins[i]);
            break;
        }
    }

    RETURN_HR_IF(E_INVALIDARG, nullptr == selectedPin);
    // the above KSMidiDeviceEnum is a temporary workaround until we can pass in the SWD to this code

    std::unique_ptr<KSMidiInDevice> midiDevice(new (std::nothrow) KSMidiInDevice());
    RETURN_IF_NULL_ALLOC(midiDevice);

    // Cyclic indicates the device can support cyclic buffering, and cyclic buffering is always UMP so UMP will always be true.
    // UMP can be supported with and without cyclic buffering, so we use UMP if it's available.
    // Finally, if cyclic and UMP arent supported, then it's going to fall back standard midi 1
    RETURN_IF_FAILED(midiDevice->Initialize(selectedPin->FilterName.get(), NULL, selectedPin->PinId, selectedPin->Cyclic, selectedPin->UMP, PAGE_SIZE, MmCss, this));
    m_MidiInCallback = Callback;
    m_MidiDevice = std::move(midiDevice);

    return S_OK;
}

HRESULT
CMidi2KSMidiIn::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    if (m_MidiDevice)
    {
        m_MidiDevice->Cleanup();
        m_MidiDevice.reset();
    }
    if (m_MidiInCallback)
    {
        m_MidiInCallback.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidiIn::Callback(
    PVOID data,
    UINT size,
    LONGLONG position
)
{
    return m_MidiInCallback->Callback(data, size, position);
}

