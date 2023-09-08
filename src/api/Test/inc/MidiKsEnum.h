// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

typedef class _UMP_PINS
{
public:
    wil::unique_cotaskmem_string FilterName;
    INT32 PinId {0};
    BOOL Standard {FALSE};
    BOOL Cyclic {FALSE};
    BOOL UMP {FALSE};
} UMP_PINS;

class KSMidiDeviceEnum
{
public:
    KSMidiDeviceEnum();
    virtual ~KSMidiDeviceEnum();
    HRESULT EnumerateFilters();
    virtual HRESULT Cleanup();

    // should have accessors, for now keeping it simple
    // and letting them be public.
    UMP_PINS m_AvailableMidiOutPins[10];
    UINT m_AvailableMidiOutPinCount {0};

    UMP_PINS m_AvailableMidiInPins[10];
    UINT m_AvailableMidiInPinCount {0};
};
