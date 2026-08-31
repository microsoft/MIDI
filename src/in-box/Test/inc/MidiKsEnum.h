// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

// minmidi supports up to 20 ump filters and 20 bytestream filters,
// each filter supports 1 in and 1 out pin.
// So, worst case for now would be the minmidi 40 + whatever 3rd
// party peripherals happen to be in the test system, so add an
// extra 10
#define MAX_PINS 50

typedef class _UMP_PINS
{
public:
    wil::unique_cotaskmem_string FilterName;
    INT32 PinId {0};
    MidiTransport TransportCapability {MidiTransport_Invalid};
} UMP_PINS;

class KSMidiDeviceEnum
{
public:
    KSMidiDeviceEnum();
    virtual ~KSMidiDeviceEnum();
    HRESULT EnumerateFilters();
    virtual HRESULT Shutdown();

    // should have accessors, for now keeping it simple
    // and letting them be public.
    UMP_PINS m_AvailableMidiOutPins[MAX_PINS];
    UINT m_AvailableMidiOutPinCount {0};

    UMP_PINS m_AvailableMidiInPins[MAX_PINS];
    UINT m_AvailableMidiInPinCount {0};
};
