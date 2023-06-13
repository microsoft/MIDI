// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.ksabstraction.h"


_Use_decl_annotations_
HRESULT
CMidi2KSMidiOut::Initialize(
    LPCWSTR Device,
    DWORD * MmCss
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmCss);

    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(Device, "Device"),
        TraceLoggingHexUInt32(*MmCss, "MmCss Id")
        );

    // the below KSMidiDeviceEnum is a temporary workaround until we can pass in the SWD to this code
    KSMidiDeviceEnum devices;
    devices.EnumerateFilters();

    UMP_PINS *selectedPin = nullptr;

    for (UINT i = 0; i < devices.m_AvailableMidiOutPinCount; i++)
    {
        if (0 == _wcsicmp(devices.m_AvailableMidiOutPins[i].FilterName.get(), Device))
        {
            selectedPin = &(devices.m_AvailableMidiOutPins[i]);
            break;
        }
    }
    RETURN_HR_IF(E_INVALIDARG, nullptr == selectedPin);
    // the above KSMidiDeviceEnum is a temporary workaround until we can pass in the SWD to this code

    std::unique_ptr<KSMidiOutDevice> midiDevice(new (std::nothrow) KSMidiOutDevice());
    RETURN_IF_NULL_ALLOC(midiDevice);

    // Cyclic indicates the device can support cyclic buffering, and cyclic buffering is always UMP so UMP will always be true.
    // UMP can be supported with and without cyclic buffering, so we use UMP if it's available.
    // Finally, if cyclic and UMP arent supported, then it's going to fall back standard midi 1
    RETURN_IF_FAILED(midiDevice->Initialize(selectedPin->FilterName.get(), NULL, selectedPin->PinId, selectedPin->Cyclic, selectedPin->UMP, PAGE_SIZE, MmCss));
    m_MidiDevice = std::move(midiDevice);

    return S_OK;
}

HRESULT
CMidi2KSMidiOut::Cleanup()
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

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSMidiOut::SendMidiMessage(
    PVOID data,
    UINT size,
    LONGLONG position
)
{
    return m_MidiDevice->WriteMidiData(data, size, position);
}

